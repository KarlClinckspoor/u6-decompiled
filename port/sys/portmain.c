/* replaces C0M.OBJ's start-up: the port's main. It finds the game, sets up the files and
 * the session (black box recording or a replay), opens the window and runs the game's own
 * main (renamed game_main by compat.h).
 *
 *   u6 [--data DIR] [--home DIR] [--replay REC] [--record DIR] [--no-recording]
 *      [--speed N] [--hidden] [--scale N] [--fullscreen] [--no-aspect] [--no-integer]
 *      [--no-audio] [--screenshot FILE] [--settings] [--new-avatar SPEC] [--help]
 */
#include "session.h"
#include "dosfs.h"
#include "dosmem.h"
#include "dosint.h"
#include "osi.h"
#include "../replay/replay.h"
#include "../platform/plat.h"
#include "../platform/hostfs.h"
#include "../ui/overlay.h"
#include "../ui/settings.h"
#include "../ui/newchar.h"
#include "../gfx/mcga.h"
#include "../gfx/vgadac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int game_main(int argc, char **argv);
uint64_t port_game_digest(void);

static uint8_t font[2048];
static int quit_after_replay = 1;
static const char *screenshot_path;
static int hidden;
void port_exit(int code);
static uint32_t run_ms;	/* --run-ms: end the session after this long (tests) */

/* files that mark the game folder */
static const char *const marks[] = {"MAPTILES.VGA", "U6PAL", "BASETILE", "OBJTILES.VGA", 0};

static int is_game_folder(const char *dir)
{
	int i;
	if (!dir || !dir[0] || !hostfs_is_dir(dir))
		return 0;
	if (dosfs_init(dir, plat_home_dir()) != 0)
		return 0;
	for (i = 0; marks[i]; i++)
		if (!dosfs_exists(marks[i]))
			return 0;
	return 1;
}

static const char *find_game(const char *given)
{
	static const char *const guesses[] = {
		".", "gamedata", "ULTIMA6", "Ultima 6",
#ifdef _WIN32
		"C:\\GOG Games\\Ultima 6", "C:\\Program Files (x86)\\GOG Galaxy\\Games\\Ultima 6",
		"C:\\Program Files (x86)\\GOG.com\\Ultima 6", "C:\\GOG Games\\Ultima VI",
#endif
		0};
	static char buf[1100];
	const char *env = getenv("U6_DATA");
	int i;
	if (given)
		return is_game_folder(given) ? given : 0;
	if (env && is_game_folder(env))
		return env;
	if (settings.data[0] && is_game_folder(settings.data))
		return settings.data;
	for (i = 0; guesses[i]; i++)
		if (is_game_folder(guesses[i]))
			return guesses[i];
	for (i = 0; guesses[i]; i++) {
		hostfs_join(buf, sizeof buf, plat_base_dir(), guesses[i]);
		if (is_game_folder(buf))
			return buf;
	}
	return 0;
}

/* END.EXE / ULTIMA6.EXE: the endgame and the main menu are other programs, not ported */
static void on_exec(const char *path, int argc, const char *const *argv)
{
	(void)argc;
	(void)argv;
	plat_log("the game asked to run %s, which is not part of the port: the session ends", path);
	port_exit(0);
}

static void write_screenshot(const char *path)
{
	/* a 320x200 8-bit BMP of what is on the screen */
	static uint8_t px[320 * 200], pal[768];
	FILE *f;
	uint8_t hdr[54 + 1024];
	int y, i;
	uint32_t size = 54 + 1024 + 320 * 200;
	if (plat_video_capture(px, pal) != 0 || !(f = fopen(path, "wb")))
		return;
	memset(hdr, 0, sizeof hdr);
	hdr[0] = 'B'; hdr[1] = 'M';
	hdr[2] = (uint8_t)size; hdr[3] = (uint8_t)(size >> 8); hdr[4] = (uint8_t)(size >> 16);
	hdr[10] = (uint8_t)(54 + 1024); hdr[11] = (uint8_t)((54 + 1024) >> 8);
	hdr[14] = 40; hdr[18] = 0x40; hdr[19] = 1; hdr[22] = 200; hdr[26] = 1; hdr[28] = 8;
	hdr[47] = 1;	/* 256 colours */
	for (i = 0; i < 256; i++) {
		hdr[54 + i * 4 + 2] = (uint8_t)(pal[i * 3] << 2 | pal[i * 3] >> 4);
		hdr[54 + i * 4 + 1] = (uint8_t)(pal[i * 3 + 1] << 2 | pal[i * 3 + 1] >> 4);
		hdr[54 + i * 4 + 0] = (uint8_t)(pal[i * 3 + 2] << 2 | pal[i * 3 + 2] >> 4);
	}
	fwrite(hdr, 1, sizeof hdr, f);
	for (y = 199; y >= 0; y--)
		fwrite(px + y * 320, 1, 320, f);
	fclose(f);
}

/* Called by the game's exit (compat.h renames exit to port_exit). */
void port_exit(int code)
{
	if (rp_mode == RP_REPLAY || rp_finished())
		plat_log("replay: %u input reads, %u of %u checkpoints compared, %s", rp_call_count(),
			rp_checks_compared(), rp_checks_total(), rp_diverged() ? "DIFFERENT from the recording" : "all identical");
	if (screenshot_path)
		write_screenshot(screenshot_path);
	session_end();
	plat_shutdown();
	exit(rp_diverged() ? 3 : code);
}

/* Replays: when the recording runs out, stop (tests) or let the player take over. */
static void replay_watch(void)
{
	static int reported;
	if (rp_mode == RP_REPLAY && rp_finished() && !reported) {
		reported = 1;
		if (quit_after_replay)
			port_exit(0);
		session_take_over();
	}
}

static void pump_hook_chain(void);
static void test_options(void);

/* --keys "MS:KEY MS:KEY ...": keys typed at those times (tests; the recording has them as
 * the game read them). KEY: a character, or up down left right enter esc space bksp tab,
 * f1..f12. */
static const char *key_script;
static const char scan_row[] = "1234567890-=\0\0qwertyuiop[]\0\0asdfghjkl;'`\0\\zxcvbnm,./";

static uint16_t bios_word(const char *k, size_t n)
{
	static const struct { const char *name; uint16_t w; } named[] = {
		{"up", 0x4800}, {"down", 0x5000}, {"left", 0x4B00}, {"right", 0x4D00},
		{"enter", 0x1C0D}, {"esc", 0x011B}, {"space", 0x3920}, {"bksp", 0x0E08}, {"tab", 0x0F09},
		{0, 0}};
	int i;
	if (n == 1) {
		char c = *k, lc = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
		for (i = 0; i < (int)sizeof scan_row - 1; i++)
			if (scan_row[i] && scan_row[i] == lc)
				return (uint16_t)((i + 2) << 8 | (unsigned char)c);
		return (unsigned char)c;
	}
	for (i = 0; named[i].name; i++)
		if (strlen(named[i].name) == n && !strncmp(named[i].name, k, n))
			return named[i].w;
	if (n >= 2 && (k[0] == 'f' || k[0] == 'F')) {
		int f = atoi(k + 1);
		if (f >= 1 && f <= 10)
			return (uint16_t)((0x3A + f) << 8);
		if (f == 11 || f == 12)
			return (uint16_t)((0x57 + f - 11) << 8);
	}
	return 0;
}

static void run_key_script(void)
{
	while (key_script && *key_script) {
		const char *p = key_script, *colon, *end;
		unsigned long at;
		while (*p == ' ')
			p++;
		if (!*p) {
			key_script = 0;
			return;
		}
		at = strtoul(p, 0, 10);
		if (plat_real_ms() < at)
			return;
		colon = strchr(p, ':');
		if (!colon) {
			key_script = 0;
			return;
		}
		end = strchr(colon + 1, ' ');
		if (!end)
			end = colon + 1 + strlen(colon + 1);
		plat_push_key(bios_word(colon + 1, (size_t)(end - colon - 1)));
		key_script = end;
	}
}

static void usage(void)
{
	printf("Ultima VI, native port\n\n"
		"  --data DIR          the game folder (remembered)\n"
		"  --home DIR          where settings, saved games and recordings go\n"
		"  --replay REC        play a recording (a folder, or a name in recordings/)\n"
		"  --speed N           replay speed: 1, 2, 4, 8, or 0 for as fast as possible\n"
		"  --continue          after a replay ends, keep playing (default: quit)\n"
		"  --record DIR        record this session into DIR\n"
		"  --no-recording      no black box recording this time\n"
		"  --hidden            no window (replays, tests)\n"
		"  --screenshot FILE   write the screen as a BMP when the game ends\n"
		"  --run-ms N          end the session after N ms (tests)\n"
		"  --keys SCRIPT       type keys at given times (tests): \"1500:up 2000:a\"\n"
		"  --scale N, --fullscreen, --no-aspect, --no-integer, --no-audio\n"
		"  --settings          open the settings screen before the game\n"
		"  --new-avatar SPEC   when the saved game has no character yet, make one without\n"
		"                      the creation screen: NAME[,M|F[,PORTRAIT 1-6[,VIRTUE 1-8]]]\n"
		"F11 opens the settings screen in the game.\n");
}

int main(int argc, char **argv)
{
	const char *data = 0, *home = 0, *replay = 0, *record = 0, *new_avatar = 0;
	int recording = -1, speed = -1, audio = 1, settings_now = 0, i;
	int scale = 0, fullscreen = -1, aspect = -1, integer = -1, first_run;
	PlatConfig cfg;
	const char *game;
	char *gargv[2];
	char homebuf[1100];

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
#define ARG (i + 1 < argc ? argv[++i] : (usage(), exit(2), (char *)0))
		if (!strcmp(a, "--data")) data = ARG;
		else if (!strcmp(a, "--home")) home = ARG;
		else if (!strcmp(a, "--replay")) replay = ARG;
		else if (!strcmp(a, "--record")) record = ARG;
		else if (!strcmp(a, "--no-recording")) recording = 0;
		else if (!strcmp(a, "--speed")) speed = atoi(ARG);
		else if (!strcmp(a, "--continue")) quit_after_replay = 0;
		else if (!strcmp(a, "--hidden")) hidden = 1;
		else if (!strcmp(a, "--screenshot")) screenshot_path = ARG;
		else if (!strcmp(a, "--scale")) scale = atoi(ARG);
		else if (!strcmp(a, "--fullscreen")) fullscreen = 1;
		else if (!strcmp(a, "--no-aspect")) aspect = 0;
		else if (!strcmp(a, "--no-integer")) integer = 0;
		else if (!strcmp(a, "--no-audio")) audio = 0;
		else if (!strcmp(a, "--settings")) settings_now = 1;
		else if (!strcmp(a, "--new-avatar")) new_avatar = ARG;
		else if (!strcmp(a, "--keys")) key_script = ARG;
		else if (!strcmp(a, "--run-ms")) run_ms = (uint32_t)atol(ARG);
		else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 0; }
		else { fprintf(stderr, "unknown option %s\n", a); usage(); return 2; }
#undef ARG
	}

	settings_defaults();
	if (!home) {
		snprintf(homebuf, sizeof homebuf, "%s", plat_home_dir());
		home = homebuf;
	}
	hostfs_mkdirs(home);
	first_run = !settings_load(home);
	session_set_home(home);
	game = find_game(data);
	if (!game) {
		fprintf(stderr, "Ultima VI's files were not found. Start the port with --data <the game folder> "
			"(the folder with MAPTILES.VGA, U6PAL, ...).\n");
		return 1;
	}
	if (data || !settings.data[0]) {
		snprintf(settings.data, sizeof settings.data, "%s", game);
		settings_save();
	}
	memset(&cfg, 0, sizeof cfg);
	cfg.scale = scale ? scale : settings.scale;
	cfg.fullscreen = fullscreen >= 0 ? fullscreen : settings.fullscreen;
	cfg.aspect = aspect >= 0 ? aspect : settings.aspect;
	cfg.integer_scale = integer >= 0 ? integer : settings.integer_scale;
	cfg.audio = audio && !hidden;
	cfg.volume = settings.sound ? settings.volume : 0;
	cfg.hidden = hidden;
	cfg.title = "Ultima VI";
	if (plat_init(&cfg) != 0)
		return 1;

	/* the overlay draws with the game's own 8x8 font; until the game sets its video mode
	 * the screen is black, with the game's palette for the overlay to pick colours from */
	dosfs_init(settings.data, home);
	dosmem_init();
	{
		static uint8_t black[320 * 200], pal[768];
		int16_t h;
		uint16_t got = 0;
		if (dosfs_open("U6.CH", 0, &h) == 0) {
			dosfs_read(h, font, sizeof font, &got);
			dosfs_close(h);
		}
		if (dosfs_open("U6PAL", 0, &h) == 0) {
			dosfs_read(h, pal, sizeof pal, &got);
			dosfs_close(h);
		}
		plat_video_source(black, pal);
		overlay_init(font);
	}
	plat_set_pump_hook(pump_hook_chain);
	overlay_set_idle_hook(test_options);

	if (!hidden && (settings_now || ((settings.settings_at_start || first_run) && !replay)))
		overlay_run_modal();

	/* U.EXE's part: a saved game without a character gets one first (before the session
	 * starts, so that a recording's copy of the saved game has it) */
	if (!replay && newchar_needed()) {
		int r;
		if (new_avatar)
			r = newchar_quick(new_avatar);
		else if (hidden) {
			fprintf(stderr, "the saved game has no character yet: start with a window, or "
				"give one with --new-avatar NAME\n");
			r = -1;
		} else
			r = newchar_run(font);
		if (r != 0) {
			plat_shutdown();
			return r < 0 && plat_quit_requested() ? 0 : 1;
		}
	}

	if (session_start(home, replay, record, !replay && (recording < 0 ? settings.recording : recording),
			argv[0]) != 0)
		return 1;
	if (dosfs_init(settings.data, session_game_home()) != 0) {
		fprintf(stderr, "cannot use the game folder %s\n", settings.data);
		return 1;
	}
	dosmem_init();
	dos_set_exec_handler(on_exec);
	dos_exit_hook = port_exit;
	if (speed >= 0)
		session_set_replay_speed(speed);
	else if (hidden)
		session_set_replay_speed(0);
	osi_replay_tick_hook = session_pace_tick;
	rp_set_digest(port_game_digest);


	gargv[0] = "GAME.EXE";
	gargv[1] = 0;
	port_exit(game_main(1, gargv));
	return 0;
}

/* the overlay's pump hook (set by overlay_init) runs first, then the replay's end check */
extern void overlay_pump_hook(void);
static void test_options(void)
{
	run_key_script();
	if (run_ms && plat_real_ms() >= run_ms)
		port_exit(0);
}

static void pump_hook_chain(void)
{
	overlay_pump_hook();
	replay_watch();
	test_options();
	if (plat_quit_requested())
		port_exit(0);
}
