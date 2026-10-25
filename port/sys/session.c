/* replaces nothing: the session around the game: where its files are, the black box
 * recording of every session, replaying a recording, and what the overlay asks for
 * (overlay.h).
 *
 * Recording: each session (unless --no-recording or the setting is off) records its
 * inputs to recordings/YYYYMMDD-HHMMSS/RECORD.U6R in the home directory, with stage/, a
 * copy of the saved game (SAVEGAME/) the session started from. The newest five are kept.
 * Replaying (--replay DIR, or the overlay's Replay tab): the saved game is copied from
 * stage/ into a fresh temporary home, so the replay never touches the player's own saves,
 * and the inputs come from RECORD.U6R.
 */
#include "session.h"
#include "../replay/replay.h"
#include "../platform/plat.h"
#include "../platform/hostfs.h"
#include "../ui/overlay.h"
#include "../ui/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define KEEP_RECORDINGS 5

static char home[1024];        /* the port's home directory */
static char game_home[1024];   /* the directory laid over the game folder (home, or a replay's) */
static char rec_dir[1024];     /* this session's recording, if any */
static char replay_dir[1024];  /* the recording being replayed, if any */
static char self_path[1024];
static int replay_speed = 1;
static uint32_t pace_first_tick, pace_start_ms, pace_have;
static int took_over;

const char *session_home(void) { return home; }
const char *session_game_home(void) { return game_home; }

int session_is_replaying(void) { return rp_mode == RP_REPLAY && !rp_finished(); }
int session_is_recording(void) { return rp_mode == RP_RECORD; }

const char *session_status(void)
{
	static char buf[64];
	if (rp_mode == RP_REPLAY) {
		const char *name = strrchr(replay_dir, '/');
		name = name ? name + 1 : replay_dir;
		if (rp_diverged())
			snprintf(buf, sizeof buf, "Replay DIFFERS from recording");
		else if (rp_finished())
			snprintf(buf, sizeof buf, "Replay ended (%u checks ok)", rp_checks_compared());
		else
			snprintf(buf, sizeof buf, "Replaying %.20s", name);
	} else if (rp_mode == RP_RECORD) {
		const char *name = strrchr(rec_dir, '/');
		snprintf(buf, sizeof buf, "Recording %.20s", name ? name + 1 : rec_dir);
	} else if (took_over) {
		snprintf(buf, sizeof buf, "Took over from the replay");
	} else {
		snprintf(buf, sizeof buf, "Not recording");
	}
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
		snprintf(p, sizeof p, "%s/recordings/%s/RECORD.U6R", home, l.names[i]);
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

static void stage_savegame(const char *stage)
{
	char src[1100];
	if (path_ci(src, sizeof src, game_home, "SAVEGAME") && hostfs_is_dir(src))
		hostfs_copy_tree(src, stage);
	else if (path_ci(src, sizeof src, settings.data, "SAVEGAME") && hostfs_is_dir(src))
		hostfs_copy_tree(src, stage);
	else
		hostfs_mkdirs(stage);
}

int session_start(const char *home_dir, const char *replay, const char *record, int black_box,
	const char *argv0)
{
	snprintf(home, sizeof home, "%s", home_dir);
	snprintf(game_home, sizeof game_home, "%s", home_dir);
	snprintf(self_path, sizeof self_path, "%s", argv0 ? argv0 : "u6");
	if (replay && replay[0]) {
		char f[1100], stage[1100], dst[1100];
		const char *tmp;
		snprintf(replay_dir, sizeof replay_dir, "%s", replay);
		if (!strchr(replay, '/') && !strchr(replay, '\\'))	/* a name from recordings/ */
			snprintf(replay_dir, sizeof replay_dir, "%s/recordings/%s", home, replay);
		hostfs_join(f, sizeof f, replay_dir, "RECORD.U6R");
		hostfs_join(stage, sizeof stage, replay_dir, "stage");
		tmp = hostfs_temp_dir("u6replay-");
		snprintf(game_home, sizeof game_home, "%s", tmp);
		hostfs_join(dst, sizeof dst, game_home, "SAVEGAME");
		hostfs_copy_tree(stage, dst);
		if (rp_start_replay(f) != 0) {
			plat_log("cannot read the recording %s", f);
			return -1;
		}
		plat_log("replaying %s", replay_dir);
		return 0;
	}
	if (record || black_box) {
		char stage[1100], f[1100], recs[1100];
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
		hostfs_join(f, sizeof f, rec_dir, "RECORD.U6R");
		if (rp_start_record(f) != 0)
			plat_log("cannot write the recording %s", f);
		if (!record || !record[0])
			prune_recordings();
	}
	return 0;
}

void session_end(void)
{
	if (rp_mode == RP_RECORD)
		rp_flush();
	if (replay_dir[0] && game_home[0] && strcmp(game_home, home))
		hostfs_remove_tree(game_home);
}

void session_play_recording(const char *name)
{
	char arg[1100];
	char *argv[4];
	snprintf(arg, sizeof arg, "%s", name);
	argv[0] = self_path;
	argv[1] = "--replay";
	argv[2] = arg;
	argv[3] = 0;
	session_end();
	plat_shutdown();
	session_exec(self_path, argv);
}

void session_take_over(void)
{
	if (rp_mode == RP_REPLAY) {
		rp_take_over();
		took_over = 1;
	}
}

int session_replay_speed(void) { return replay_speed; }
void session_set_replay_speed(int speed) { replay_speed = speed; pace_have = 0; }

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
}

/* Replay pacing: called with each timer tick value the replayed game reads; waits so
 * that the recording plays at replay_speed times its own pace (0: no waiting). */
void session_pace_tick(uint32_t tick)
{
	uint32_t due, now;
	if (rp_mode != RP_REPLAY || replay_speed == 0)
		return;
	now = plat_ms();
	if (!pace_have || tick < pace_first_tick) {
		pace_have = 1;
		pace_first_tick = tick;
		pace_start_ms = now;
		return;
	}
	due = pace_start_ms + (uint32_t)((uint64_t)(tick - pace_first_tick) * 10000u / 182u / (uint32_t)replay_speed);
	while ((int32_t)(due - now) > 0 && !plat_quit_requested()) {
		uint32_t wait = due - now;
		plat_pump();
		plat_sleep_ms(wait > 10 ? 10 : wait);
		now = plat_ms();
		if (rp_mode != RP_REPLAY || replay_speed == 0)
			break;
	}
}

/* ---- exec ---- */
#ifdef _WIN32
#include <process.h>
void session_exec(const char *path, char **argv)
{
	_execv(path, (const char *const *)argv);
	exit(0);
}
#else
#include <unistd.h>
void session_exec(const char *path, char **argv)
{
	execv(path, argv);
	exit(0);
}
#endif
