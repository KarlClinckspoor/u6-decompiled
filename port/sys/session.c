/* replaces nothing: the session around the game: where its files are, the black box
 * recording of every session, replaying a recording, and what the overlay asks for
 * (overlay.h). Recording and replaying are Exhume's runtime/replay/replay.c, the code the
 * replay DOS build runs, so the port's recordings are in its format (RECORD.OUT) and a
 * recording of either replays in the port (port/include/rpport.h, port/game/rpport.c).
 *
 * The black box: each player's session (unless --no-recording or the setting is off) is
 * recorded to recordings/YYYYMMDD-HHMMSS/RECORD.OUT in the home directory, with stage/, a copy
 * of the saved game (SAVEGAME/) the session started from. No dumps are written and F9 does
 * not end it; the game's exit, the window's closing and a fault close it (rp_blackbox_close).
 * The newest five are kept.
 * --record DIR: the same in DIR, with the dumps (STATE.OUT), ended by F9 (a test's).
 * Replaying (--replay DIR, or the overlay's Replay tab): the saved game is copied from stage/
 * into a fresh temporary home, so the replay never touches the player's own saves, and the
 * inputs come from RECORD.OUT; it writes no dumps, and the port exits at its end (or the
 * player takes over). With --out OUT the replay runs in OUT instead and writes its dumps
 * there, to compare with the recording's (tools/rp.py port-self).
 */
#include "session.h"
#include "osi.h"
#include "adlib.h"
#include "../platform/plat.h"
#include "../platform/hostfs.h"
#include "../ui/overlay.h"
#include "../ui/settings.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define KEEP_RECORDINGS 5

/* port/game/rpport.c: replay.c's mode and files; replay.c's black box end and take-over */
void u6_rp_set_off(void);
void u6_rp_set_record(const char *dir);
void u6_rp_set_blackbox(const char *file);
void u6_rp_set_replay(const char *file, int watch);
void rp_blackbox_close(int crashed);
int rp_take_over(void);

enum { S_OFF, S_RECORD, S_BLACKBOX, S_REPLAY };

static char home[1024];        /* the port's home directory */
static char game_home[1024];   /* the directory laid over the game folder (home, or a replay's) */
static char rec_dir[1024];     /* this session's recording, if any */
static char rec_file[1100];    /* its RECORD.OUT */
static char replay_dir[1024];  /* the recording being replayed, if any */
static char replay_file[1100];
static char self_path[1024];
static int mode = S_OFF;
static int temp_home;          /* game_home is a temporary folder, removed at the end */
static int took_over;

const char *session_home(void) { return home; }
void session_set_home(const char *dir)
{
	snprintf(home, sizeof home, "%s", dir);
	snprintf(game_home, sizeof game_home, "%s", dir);
}
const char *session_game_home(void) { return game_home; }

int session_is_replaying(void) { return mode == S_REPLAY && osi_replaying(); }
int session_is_recording(void) { return mode == S_RECORD || mode == S_BLACKBOX; }

static const char *base_name(const char *p)
{
	const char *s = strrchr(p, '/'), *b = strrchr(p, '\\');
	if (b > s)
		s = b;
	return s ? s + 1 : p;
}

const char *session_status(void)
{
	static char buf[64];
	if (took_over)
		snprintf(buf, sizeof buf, "Took over from the replay");
	else if (mode == S_REPLAY)
		snprintf(buf, sizeof buf, "Replaying %.20s", base_name(replay_dir));
	else if (mode == S_RECORD || mode == S_BLACKBOX)
		snprintf(buf, sizeof buf, "Recording %.20s", base_name(rec_dir));
	else
		snprintf(buf, sizeof buf, "Not recording");
	return buf;
}

/* ---- recordings ---- */
struct reclist { char names[32][64]; int n; };

static void add_rec(const char *name, int is_dir, void *ud)
{
	struct reclist *l = ud;
	if (is_dir && l->n < 32 && strlen(name) == 15 && name[8] == '-')
		snprintf(l->names[l->n++], 64, "%s", name);
}

static int cmp_desc(const void *a, const void *b) { return strcmp((const char *)b, (const char *)a); }

static void list_recordings(struct reclist *l)
{
	char dir[1100];
	hostfs_join(dir, sizeof dir, home, "recordings");
	l->n = 0;
	hostfs_list(dir, add_rec, l);
	qsort(l->names, (size_t)l->n, 64, cmp_desc);
}

int session_recordings(char names[][64], int max)
{
	struct reclist l;
	int i, n = 0;
	list_recordings(&l);
	for (i = 0; i < l.n && n < max; i++) {
		char p[1100];
		if (rec_dir[0] && strstr(rec_dir, l.names[i]))
			continue;	/* not the one being written */
		snprintf(p, sizeof p, "%s/recordings/%s/RECORD.OUT", home, l.names[i]);
		if (hostfs_exists(p))
			snprintf(names[n++], 64, "%s", l.names[i]);
	}
	return n;
}

static void prune_recordings(void)
{
	struct reclist l;
	int i;
	list_recordings(&l);
	for (i = KEEP_RECORDINGS; i < l.n; i++) {
		char p[1100];
		snprintf(p, sizeof p, "%s/recordings/%s", home, l.names[i]);
		hostfs_remove_tree(p);
	}
}

/* the saved game the session starts from: the home's SAVEGAME, else the game folder's */
struct find_ctx { const char *want; char found[256]; };

static void find_ci(const char *name, int is_dir, void *ud)
{
	struct find_ctx *c = ud;
	(void)is_dir;
	if (!c->found[0]) {
		const char *a = name, *b = c->want;
		while (*a && *b && (*a | 32) == (*b | 32)) {
			a++;
			b++;
		}
		if (!*a && !*b)
			snprintf(c->found, sizeof c->found, "%s", name);
	}
}

/* path of the entry `name` in dir, matched without case; 0 if none */
static int path_ci(char *out, size_t size, const char *dir, const char *name)
{
	struct find_ctx c;
	c.want = name;
	c.found[0] = 0;
	hostfs_list(dir, find_ci, &c);
	if (!c.found[0])
		return 0;
	hostfs_join(out, size, dir, c.found);
	return 1;
}

/* the overlay's list of game folder files the game deleted (dosfs) goes with the copy */
#define DELETED_LIST ".dosfs-deleted"

static void stage_savegame(const char *stage)
{
	char src[1100], dst[1100];
	hostfs_join(src, sizeof src, game_home, DELETED_LIST);
	hostfs_join(dst, sizeof dst, rec_dir, DELETED_LIST);	/* beside stage/ */
	if (hostfs_exists(src))
		hostfs_copy_file(src, dst);
	if (path_ci(src, sizeof src, game_home, "SAVEGAME") && hostfs_is_dir(src))
		hostfs_copy_tree(src, stage);
	else if (path_ci(src, sizeof src, settings.data, "SAVEGAME") && hostfs_is_dir(src))
		hostfs_copy_tree(src, stage);
	else
		hostfs_mkdirs(stage);
}

/* whether f starts as a recording replay.c reads (its magic, rpport.h's RP_MAGIC_TAG) */
static int is_recording(const char *f)
{
	char b[4];
	FILE *fp = fopen(f, "rb");
	int ok = fp && fread(b, 1, 4, fp) == 4 && !memcmp(b, "U6GR", 4);
	if (fp)
		fclose(fp);
	return ok;
}

/* A fault in the port: the recording's buffered runs are written and its file closed, so the
 * black box holds the session up to the fault, then the fault goes on as it would have. */
static void on_fault(int sig)
{
	rp_blackbox_close(1);
	signal(sig, SIG_DFL);
	raise(sig);
}

static void catch_faults(void)
{
	signal(SIGSEGV, on_fault);
	signal(SIGFPE, on_fault);
	signal(SIGILL, on_fault);
	signal(SIGABRT, on_fault);
}

int session_start_file(const char *home_dir, const char *file)
{
	session_set_home(home_dir);
	if (!is_recording(file)) {
		plat_log("%s is not a recording", file);
		return -1;
	}
	snprintf(replay_file, sizeof replay_file, "%s", file);
	snprintf(replay_dir, sizeof replay_dir, "%s", file);
	u6_rp_set_replay(replay_file, 0);
	osi_set_replay(1);
	osi_set_replay_speed(0);
	mode = S_REPLAY;
	return 0;
}

int session_start(const char *home_dir, const char *replay, const char *replay_out,
	const char *record, int black_box, const char *argv0)
{
	session_set_home(home_dir);
	snprintf(self_path, sizeof self_path, "%s", argv0 ? argv0 : "u6");
	u6_rp_set_off();
	if (replay && replay[0]) {
		char stage[1100], dst[1100], f[1100];
		snprintf(replay_dir, sizeof replay_dir, "%s", replay);
		if (!strchr(replay, '/') && !strchr(replay, '\\'))	/* a name from recordings/ */
			snprintf(replay_dir, sizeof replay_dir, "%s/recordings/%s", home, replay);
		hostfs_join(replay_file, sizeof replay_file, replay_dir, "RECORD.OUT");
		if (!is_recording(replay_file)) {
			plat_log("cannot read the recording %s", replay_file);
			return -1;
		}
		if (replay_out && replay_out[0]) {
			snprintf(game_home, sizeof game_home, "%s", replay_out);
			hostfs_mkdirs(game_home);
			if (path_ci(dst, sizeof dst, game_home, "SAVEGAME"))
				hostfs_remove_tree(dst);
		} else {
			snprintf(game_home, sizeof game_home, "%s", hostfs_temp_dir("u6replay-"));
			temp_home = 1;
		}
		hostfs_join(stage, sizeof stage, replay_dir, "stage");
		hostfs_join(dst, sizeof dst, game_home, "SAVEGAME");
		hostfs_copy_tree(stage, dst);
		hostfs_join(f, sizeof f, replay_dir, DELETED_LIST);
		hostfs_join(dst, sizeof dst, game_home, DELETED_LIST);
		if (hostfs_exists(f))
			hostfs_copy_file(f, dst);
		u6_rp_set_replay(replay_file, !(replay_out && replay_out[0]));
		osi_set_replay(1);
		mode = S_REPLAY;
		plat_log("replaying %s", replay_dir);
		return 0;
	}
	if (record || black_box) {
		char stage[1100], recs[1100];
		if (record && record[0]) {
			snprintf(rec_dir, sizeof rec_dir, "%s", record);
		} else {
			time_t t = time(NULL);
			struct tm *tm = localtime(&t);
			char name[32];
			strftime(name, sizeof name, "%Y%m%d-%H%M%S", tm);
			hostfs_join(recs, sizeof recs, home, "recordings");
			hostfs_mkdirs(recs);
			hostfs_join(rec_dir, sizeof rec_dir, recs, name);
		}
		hostfs_mkdirs(rec_dir);
		hostfs_join(stage, sizeof stage, rec_dir, "stage");
		stage_savegame(stage);
		hostfs_join(rec_file, sizeof rec_file, rec_dir, "RECORD.OUT");
		if (record && record[0]) {
			u6_rp_set_record(rec_dir);
			mode = S_RECORD;
		} else {
			u6_rp_set_blackbox(rec_file);
			mode = S_BLACKBOX;
			prune_recordings();
		}
		catch_faults();
	}
	return 0;
}

void session_end(void)
{
	rp_blackbox_close(0);	/* a recording's last runs and its header (none: nothing) */
	if (temp_home && game_home[0] && strcmp(game_home, home)) {
		hostfs_remove_tree(game_home);
		temp_home = 0;
	}
}

/* restarts the port with these options (and the same home) */
static void restart(const char *opt, const char *val)
{
	char *argv[6];
	int n = 0;
	argv[n++] = self_path;
	argv[n++] = "--home";
	argv[n++] = home;
	if (opt)
		argv[n++] = (char *)opt;
	if (val)
		argv[n++] = (char *)val;
	argv[n] = 0;
	session_end();
	plat_shutdown();
	session_exec(self_path, argv);
}

/* GAME.EXE ran ULTIMA6.EXE (no character yet): the port starts again with U.EXE */
void session_restart_menu(void)
{
	restart("--menu", 0);
}

void session_play_recording(const char *name)
{
	char arg[1100];
	snprintf(arg, sizeof arg, "%s", name);
	restart("--replay", arg);
}

/* New game: the home's saved game (and its list of deleted game folder files) is moved to
 * backups/, so the game folder's own, as installed, shows through; without a character
 * there, the creation screen comes first. */
int session_can_start_new_game(void)
{
	return mode != S_REPLAY && !took_over;
}

void session_new_game(void)
{
	char src[1100], dir[1100], dst[1100], name[64];
	time_t t = time(NULL);
	strftime(name, sizeof name, "SAVEGAME-%Y%m%d-%H%M%S", localtime(&t));
	hostfs_join(dir, sizeof dir, home, "backups");
	hostfs_mkdirs(dir);
	hostfs_join(dst, sizeof dst, dir, name);
	hostfs_mkdirs(dst);
	if (path_ci(src, sizeof src, home, "SAVEGAME") && hostfs_is_dir(src)) {
		hostfs_copy_tree(src, dst);
		hostfs_remove_tree(src);
	}
	hostfs_join(src, sizeof src, home, DELETED_LIST);
	if (hostfs_exists(src)) {
		hostfs_join(dir, sizeof dir, dst, DELETED_LIST);
		hostfs_copy_file(src, dir);
		remove(src);
	}
	plat_log("new game: the previous saved game is in %s", dst);
	restart(0, 0);
}

/* The player takes over (the overlay): replay.c reads the world from the next hook call on,
 * and the clock goes on from the replay's. Nothing is recorded after it. */
void session_take_over(void)
{
	if (mode == S_REPLAY && rp_take_over()) {
		osi_set_replay(0);
		took_over = 1;
	}
}

int session_replay_speed(void) { return osi_replay_speed(); }
void session_set_replay_speed(int speed) { osi_set_replay_speed(speed); }

void session_apply_settings(void)
{
	PlatConfig c;
	memset(&c, 0, sizeof c);
	c.scale = settings.scale;
	c.fullscreen = settings.fullscreen;
	c.aspect = settings.aspect;
	c.integer_scale = settings.integer_scale;
	c.audio = 1;
	c.volume = settings.sound ? settings.volume : 0;
	c.title = "Ultima VI";
	plat_apply_config(&c);
	adlib_set_volume(settings.music ? settings.music_volume : 0);
}

/* ---- exec ---- */
#ifdef _WIN32
#include <windows.h>
#include <process.h>
/* _execv joins the arguments with spaces: quote the ones that have spaces */
void session_exec(const char *path, char **argv)
{
	static char quoted[8][1200];
	char *args[9];
	char exe[1100];
	int i;
	if (GetModuleFileNameA(NULL, exe, sizeof exe))
		path = exe;
	for (i = 0; argv[i] && i < 8; i++) {
		if (strchr(argv[i], ' ') || !argv[i][0]) {
			snprintf(quoted[i], sizeof quoted[i], "\"%s\"", argv[i]);
			args[i] = quoted[i];
		} else
			args[i] = argv[i];
	}
	args[i] = 0;
	_execv(path, (const char *const *)args);
	exit(0);
}
#else
#include <unistd.h>
void session_exec(const char *path, char **argv)
{
#ifdef __linux__
	static char exe[1100];
	ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
	if (n > 0) {
		exe[n] = 0;
		path = exe;
	}
#endif
	execv(path, argv);
	if (!strchr(path, '/'))
		execvp(path, argv);
	exit(0);
}
#endif
