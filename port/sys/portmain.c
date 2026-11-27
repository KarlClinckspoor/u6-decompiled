/* replaces C0M.OBJ's start-up: the port's main. It finds the game, sets up the files and
 * the session (black box recording or a replay), opens the window and runs the game's own
 * main (renamed game_main by compat.h).
 *
 *   u6 [--data DIR] [--home DIR] [--replay REC [--out DIR]] [--record DIR] [--no-recording]
 *      [--dos-replay FILE] [--speed N] [--hidden] [--scale N] [--fullscreen] [--no-aspect]
 *      [--no-integer] [--no-audio] [--screenshot FILE] [--settings] [--new-avatar SPEC] [--help]
 */
#include "session.h"
#include "dosfs.h"
#include "dosmem.h"
#include "dosint.h"
#include "osi.h"
#include "fixes.h"
#include "../platform/plat.h"
#include "../platform/hostfs.h"
#include "../ui/overlay.h"
#include "../ui/settings.h"
#include "../ui/newchar.h"
#include "../gfx/mcga.h"
#include "../gfx/vgadac.h"
#include "../u/urun.h"
#include "../end/erun.h"
#include "adlib.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int game_main(int argc, char **argv);

static uint8_t font[2048];
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
		".", "gamedata", "ULTIMA6", "Ultima 6", "..", "../gamedata",
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

static int same_name(const char *path, const char *name)
{
	const char *b = path, *p;
	for (p = path; *p; p++)
		if (*p == '/' || *p == '\\' || *p == ':')
			b = p + 1;
	for (; *b && *name; b++, name++)
		if ((*b | 0x20) != (*name | 0x20))
			return 0;
	return !*b && !*name;
}

/* END.EXE's execs: none; it exits */
static void on_exec_end(const char *path, int argc, const char *const *argv)
{
	(void)argc;
	(void)argv;
	plat_log("END.EXE asked to run %s, which is not part of the port: the session ends", path);
	port_exit(0);
}

/* GAME.EXE's execs: ULTIMA6.EXE when the saved game has no character (the port starts
 * again with U.EXE's menu); END.EXE, the endgame (end.exe <config> <years> <months>
 * <days>), which the port runs in place of the game (a replay stops there: the ending is
 * not recorded) */
static void on_exec(const char *path, int argc, const char *const *argv)
{
	if (same_name(path, "ultima6.exe") && !session_is_replaying()) {
		plat_log("the game runs ULTIMA6.EXE: back to the main menu");
		session_end();
		session_restart_menu();
	}
	if (same_name(path, "end.exe") && !session_is_replaying()) {
		plat_log("the game runs END.EXE: the ending");
		session_end();
		MUSDRV_9();
		plat_speaker(0);
		dosmem_reset();	/* DOS frees GAME's memory, which GAME's main took whole */
		dos_set_exec_handler(on_exec_end);
		e_run(argc, (char **)argv);
		port_exit(0);	/* END's main returned */
	}
	plat_log("the game asked to run %s, which is not part of the port: the session ends", path);
	port_exit(0);
}

/* U.EXE's exec of GAME.EXE: back to main, which goes on with the game */
static jmp_buf u_done;
static void on_exec_u(const char *path, int argc, const char *const *argv)
{
	(void)argc;
	(void)argv;
	if (same_name(path, "game.exe"))
		longjmp(u_done, 1);
	plat_log("U.EXE asked to run %s, which is not part of the port: the session ends", path);
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

void u6_rp_set_off(void);

/* Called by the game's exit (compat.h renames exit to port_exit), and by replay.c's at the end
 * of a replay or a --record session. */
void port_exit(int code)
{
	if (screenshot_path)
		write_screenshot(screenshot_path);
	session_end();
	plat_shutdown();
	exit(code);
}

static void pump_hook_chain(void);
static void test_options(void);

/* --keys "MS:KEY MS:KEY ...": keys typed at those times (tests; the recording has them as
 * the game read them), each one once the game has taken the one before: the game's
 * KBD_GetKey(1) keeps only the last key in the buffer, so keys typed while it is busy would
 * be lost. KEY: a character, or up down left right enter esc space bksp tab, f1..f12, ^x for
 * Ctrl+x. */
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
	if (n == 2 && k[0] == '^') {	/* ^s: Ctrl+S */
		uint16_t w = bios_word(k + 1, 1);
		return (uint16_t)((w & 0xff00) | ((w & 0x1f)));
	}
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
		if (plat_real_ms() < at || plat_key_available())
			return;
		colon = strchr(p, ':');
		if (!colon) {
			key_script = 0;
			return;
		}
		end = strchr(colon + 1, ' ');
		if (!end)
			end = colon + 1 + strlen(colon + 1);
		{
			/* the mouse: mouse:X,Y moves it, click:X,Y and rclick:X,Y move it and press the
			 * left or the right button (the game's 320x200 pixels) */
			static const struct { const char *name; uint16_t buttons; } m[] = {
				{"mouse:", 0}, {"click:", 1}, {"rclick:", 2}, {0, 0}};
			int j, done = 0;
			for (j = 0; m[j].name; j++) {
				size_t n = strlen(m[j].name);
				if (!strncmp(colon + 1, m[j].name, n)) {
					int x = atoi(colon + 1 + n);
					const char *comma = strchr(colon + 1 + n, ',');
					plat_set_mouse((int16_t)x, (int16_t)(comma && comma < end ? atoi(comma + 1) : 0), m[j].buttons);
					done = 1;
					break;
				}
			}
			if (!done)
				plat_push_key(bios_word(colon + 1, (size_t)(end - colon - 1)));
		}
		key_script = end;
	}
}

static void usage(void)
{
	printf("Ultima VI, native port\n\n"
		"  --data DIR          the game folder (remembered)\n"
		"  --home DIR          where settings, saved games and recordings go\n"
		"  --replay REC        play a recording (a folder, or a name in recordings/); the port\n"
		"                      exits at its end, unless you take over (F11)\n"
		"  --out DIR           with --replay: run the replay in DIR and write its dumps\n"
		"                      (STATE.OUT) there, to compare with the recording's (tests)\n"
		"  --speed N           replay speed: 1, 2, 4, 8, or 0 for as fast as possible\n"
		"  --record DIR        record this session into DIR, with the dumps; F9 ends it (tests)\n"
		"  --dos-replay FILE   replay a RECORD.OUT (of the replay DOS build, or the port's) from\n"
		"                      the saved game in --home, writing STATE.OUT there\n"
		"  --no-recording      no black box recording this time\n"
		"  --hidden            no window (replays, tests)\n"
		"  --screenshot FILE   write the screen as a BMP when the game ends\n"
		"  --run-ms N          end the session after N ms (tests)\n"
		"  --keys SCRIPT       type keys at given times (tests): \"1500:up 2000:a\"; and the\n"
		"                      mouse, in the game's pixels: MS:mouse:X,Y, MS:click:X,Y, MS:rclick:X,Y\n"
		"  --farcore N         farcoreleft() at most N (tests: DOS's, so that DOS replays it)\n"
		"  --fixes LIST        fix the original game's faults: all, none, or names (--fixes list);\n"
		"                      off by default: the port then plays exactly as DOS\n"
		"  --enhance LIST      changes to the original's design: all, none, or names\n"
		"                      (--enhance list: moonstone-guard); off by default\n"
		"  --scale N, --fullscreen, --no-aspect, --no-integer, --no-audio\n"
		"  --settings          open the settings screen before the game\n"
		"  --menu, --game      start with the main menu (U.EXE: title, introduction,\n"
		"                      character creation), or with the game (default: the setting)\n"
		"  --new-avatar SPEC   when the saved game has no character yet, make one without\n"
		"                      the gypsy: NAME[,M|F[,PORTRAIT 1-6[,VIRTUE 1-8]]] (tests)\n"
		"  --ending Y,M,D      play the ending (END.EXE), as after Y years, M months and\n"
		"                      D days in Britannia\n"
		"F11 opens the settings screen in the game.\n");
}

int main(int argc, char **argv)
{
	const char *data = 0, *home = 0, *replay = 0, *record = 0, *new_avatar = 0, *ending = 0;
	const char *dos_replay = 0, *replay_out = 0, *fixes_opt = 0, *enhance_opt = 0;
	int menu = -1;
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
		else if (!strcmp(a, "--out")) replay_out = ARG;
		else if (!strcmp(a, "--record")) record = ARG;
		else if (!strcmp(a, "--dos-replay")) dos_replay = ARG;
		else if (!strcmp(a, "--no-recording")) recording = 0;
		else if (!strcmp(a, "--speed")) speed = atoi(ARG);
		else if (!strcmp(a, "--hidden")) hidden = 1;
		else if (!strcmp(a, "--screenshot")) screenshot_path = ARG;
		else if (!strcmp(a, "--scale")) scale = atoi(ARG);
		else if (!strcmp(a, "--fullscreen")) fullscreen = 1;
		else if (!strcmp(a, "--no-aspect")) aspect = 0;
		else if (!strcmp(a, "--no-integer")) integer = 0;
		else if (!strcmp(a, "--no-audio")) audio = 0;
		else if (!strcmp(a, "--settings")) settings_now = 1;
		else if (!strcmp(a, "--new-avatar")) new_avatar = ARG;
		else if (!strcmp(a, "--menu")) menu = 1;
		else if (!strcmp(a, "--game")) menu = 0;
		else if (!strcmp(a, "--ending")) ending = ARG;
		else if (!strcmp(a, "--keys")) {
			key_script = ARG;
			if (strstr(key_script, "click:") || strstr(key_script, "mouse:"))
				plat_test_mouse();	/* a mouse even with --hidden */
		}
		else if (!strcmp(a, "--farcore")) dos_farcore_cap = strtoul(ARG, 0, 0);
		else if (!strcmp(a, "--fixes")) {
			fixes_opt = ARG;
			if (!strcmp(fixes_opt, "list")) {
				fixes_list(stdout, KIND_FIX);
				return 0;
			}
		}
		else if (!strcmp(a, "--enhance")) {
			enhance_opt = ARG;
			if (!strcmp(enhance_opt, "list")) {
				fixes_list(stdout, KIND_ENHANCE);
				return 0;
			}
		}
		else if (!strcmp(a, "--run-ms")) run_ms = (uint32_t)atol(ARG);
		else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 0; }
		else { fprintf(stderr, "unknown option %s\n", a); usage(); return 2; }
#undef ARG
	}

	/* Exhume's replay.c (port/include/rpport.h): off until the session starts (U.EXE's part is
	 * never recorded). --dos-replay is the game alone, from the saved game in the home folder
	 * (tools/replay.py's run_port puts the session's stage there). */
	u6_rp_set_off();
	if (dos_replay) {
		replay = 0;
		record = 0;
		recording = 0;
		menu = 0;
		new_avatar = 0;
		ending = 0;
	}

	settings_defaults();
	if (!home) {
		snprintf(homebuf, sizeof homebuf, "%s", plat_home_dir());
		home = homebuf;
	}
	hostfs_mkdirs(home);
	first_run = !settings_load(home);
	session_set_home(home);
	{
		char logp[1100];
		hostfs_join(logp, sizeof logp, home, "u6port.log");
		plat_log_to(logp);
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

	game = find_game(data);
	if (!game && !hidden) {
		const char *pick;
		plat_message(data ? "Ultima VI's files are not in the folder given with --data. "
			"Choose the folder of your Ultima VI (the one with MAPTILES.VGA, U6PAL, ...)."
			: "Choose the folder of your Ultima VI (the one with MAPTILES.VGA, U6PAL, ...).");
		pick = plat_choose_folder("The folder of Ultima VI");
		if (pick && is_game_folder(pick)) {
			game = pick;
			data = pick;
		}
	}
	if (!game) {
		plat_message("Ultima VI's files were not found. Start the port with --data <the game folder> "
			"(the folder with MAPTILES.VGA, U6PAL, ...).");
		plat_shutdown();
		return 1;
	}
	if (data || !settings.data[0] || strcmp(settings.data, game)) {
		snprintf(settings.data, sizeof settings.data, "%s", game);
		settings_save();
	}

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

	if (!hidden && !dos_replay && (settings_now || ((settings.settings_at_start || first_run) && !replay)))
		overlay_run_modal();

	adlib_set_volume(settings.music ? settings.music_volume : 0);

	/* --ending: END.EXE alone, with the arguments GAME.EXE gives it */
	if (ending && !replay) {
		static char y[8], m[8], d[8];
		static char cfgstr[] = "MA";	/* mouse, AdLib (e_run adds VGA) */
		char *eargv[6];
		int ey = 0, em = 0, ed = 0;
		sscanf(ending, "%d,%d,%d", &ey, &em, &ed);
		snprintf(y, sizeof y, "%d", ey);
		snprintf(m, sizeof m, "%d", em);
		snprintf(d, sizeof d, "%d", ed);
		eargv[0] = "END.EXE";
		eargv[1] = cfgstr;
		eargv[2] = y;
		eargv[3] = m;
		eargv[4] = d;
		eargv[5] = 0;
		dosfs_init(settings.data, home);
		dosmem_init();
		dos_set_exec_handler(on_exec_end);
		dos_exit_hook = port_exit;
		e_run(5, eargv);
		port_exit(0);
	}

	/* U.EXE's part (ULTIMA6.EXE runs it first): the title, the main menu, the introduction
	 * and the gypsy; "Journey onward" (or a new character) goes on with GAME.EXE below. It
	 * comes before the session starts, so that a recording's copy of the saved game has
	 * the character. A saved game without a character always starts there, unless
	 * --new-avatar makes one (tests). */
	if (dos_replay) {
		/* the DOS recording starts in GAME.EXE, from the saved game it was made from */
	} else if (!replay && new_avatar && newchar_needed()) {
		if (newchar_quick(new_avatar) != 0) {
			plat_shutdown();
			return 1;
		}
	} else if (!replay && (menu > 0 || (menu < 0 && settings.start_menu) || newchar_needed())) {
		static char *uargv[] = {"U.EXE", "V", 0};	/* VGA: the port has MCGADRV.BIN only */
		dosfs_init(settings.data, home);
		dosmem_init();
		dos_set_exec_handler(on_exec_u);
		dos_exit_hook = port_exit;
		if (!setjmp(u_done)) {
			u_run(2, uargv);
			port_exit(0);	/* U's main returned: the player quit */
		}
		/* U ran GAME.EXE */
		MUSDRV_9();
		plat_speaker(0);
	}

	if (dos_replay) {
		if (session_start_file(home, dos_replay) != 0)
			return 1;
	} else if (session_start(home, replay, replay_out, record,
			!replay && (recording < 0 ? settings.recording : recording), argv[0]) != 0)
		return 1;
	{
		/* the fixes (fixes.h): a replay's are the recording's (a DOS recording has none);
		 * otherwise the settings' fixes=, then --fixes. Set before the game starts, so that
		 * replay.c records them in the session's header (format 5) */
		extern const char *u6_rp_replay_path;	/* port/game/rpport.c: the session's recording */
		uint32_t m = 0;
		if (dos_replay || replay) {
			if (u6_rp_replay_path && fixes_from_recording(u6_rp_replay_path, &m) != 0)
				return 1;
			/* tests: a DOS recording replayed with fixes; where none is reached, the run
			 * must stay DOS's (tools/rp.py port-check --fixes) */
			if (dos_replay && ((fixes_opt && fixes_parse(fixes_opt, &m, KIND_FIX) != 0) ||
					(enhance_opt && fixes_parse(enhance_opt, &m, KIND_ENHANCE) != 0)))
				return 1;
		} else if (fixes_parse(settings.fixes, &m, KIND_FIX) != 0 ||
				fixes_parse(settings.enhance, &m, KIND_ENHANCE) != 0 ||
				(fixes_opt && fixes_parse(fixes_opt, &m, KIND_FIX) != 0) ||
				(enhance_opt && fixes_parse(enhance_opt, &m, KIND_ENHANCE) != 0))
			return 1;
		enhance_on = m;
		fixes_log();
	}
	if (dosfs_init(settings.data, session_game_home()) != 0) {
		fprintf(stderr, "cannot use the game folder %s\n", settings.data);
		return 1;
	}
	dosmem_reset();	/* GAME.EXE starts with DOS's memory free, whatever U.EXE left in it */
	dos_set_exec_handler(on_exec);
	dos_exit_hook = port_exit;
	if (dos_replay || replay_out)
		session_set_replay_speed(0);
	else
		session_set_replay_speed(speed >= 0 ? speed : hidden ? 0 : 1);


	gargv[0] = "GAME.EXE";
	gargv[1] = 0;
	port_exit(game_main(1, gargv));
	return 0;
}

/* the overlay's pump hook (set by overlay_init) runs first, then the test options */
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
	test_options();
	if (plat_quit_requested())
		port_exit(0);
}
