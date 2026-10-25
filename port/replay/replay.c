/* replaces nothing: record and replay (replay.h).
 *
 * A recording is a folder: RECORD.U6R and stage/, a copy of the saved game the session
 * started from. RECORD.U6R:
 *
 *   "U6RP" u32 version (1)
 *   u32 nstreams, then per stream: u32 count of runs, runs of (u32 value, u32 repeat)
 *   u32 ncheck, then per checkpoint: u32 call (rp_calls when taken), u64 digest
 *
 * Values are stored as runs (value, number of times in a row): a keyboard poll that finds
 * nothing is one run however long the player waits. The timer stream stores deltas from
 * the previous reading, so a waiting game's ticks are runs too.
 *
 * Checkpoints: every RP_CHECK_EVERY calls of rp_value the port calls rp_checkpoint with a
 * digest of the game's state; a replay compares its own digest with the recorded one and
 * reports the first difference (rp_diverged).
 *
 * The file is rewritten whole (to a temporary name, then renamed) at each flush, so a
 * crash leaves the last flushed recording intact: the black box.
 */
#include "replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RP_VERSION 1

int rp_mode = RP_OFF;

typedef struct Run { uint32_t value, repeat; } Run;
typedef struct Stream {
	Run *runs;
	uint32_t n, cap;
	/* replay cursor */
	uint32_t pos, used;
	uint32_t last;	/* the previous raw value (RP_TICKS deltas) */
} Stream;

typedef struct Check { uint32_t call; uint64_t digest; } Check;

static Stream streams[RP_NSTREAMS];
static Check *checks;
static uint32_t nchecks, capchecks, check_pos;
static uint32_t rp_calls;
static int finished;
static uint32_t diverged_at;	/* call count of the first differing checkpoint, 0 = none */
static char record_path[1024];
static rp_digest_fn digest_fn;

static void push_run(Stream *s, uint32_t v)
{
	if (s->n && s->runs[s->n - 1].value == v && s->runs[s->n - 1].repeat != 0xffffffffu) {
		s->runs[s->n - 1].repeat++;
		return;
	}
	if (s->n == s->cap) {
		s->cap = s->cap ? s->cap * 2 : 256;
		s->runs = realloc(s->runs, s->cap * sizeof(Run));
	}
	s->runs[s->n].value = v;
	s->runs[s->n].repeat = 1;
	s->n++;
}

static int pop_run(Stream *s, uint32_t *v)
{
	while (s->pos < s->n && s->used >= s->runs[s->pos].repeat) {
		s->pos++;
		s->used = 0;
	}
	if (s->pos >= s->n)
		return 0;
	*v = s->runs[s->pos].value;
	s->used++;
	return 1;
}

uint32_t rp_value_(int stream, uint32_t live)
{
	Stream *s = &streams[stream];
	uint32_t v;
	rp_calls++;
	if (rp_mode == RP_RECORD) {
		if (stream == RP_TICKS) {
			push_run(s, live - s->last);
			s->last = live;
		} else {
			push_run(s, live);
		}
		v = live;
	} else {
		if (finished || !pop_run(s, &v)) {
			finished = 1;
			return live;
		}
		if (stream == RP_TICKS) {
			v = s->last + v;
			s->last = v;
		}
	}
	if (digest_fn && rp_calls % RP_CHECK_EVERY == 0)
		rp_checkpoint(digest_fn());
	return v;
}

int rp_finished(void) { return finished; }
uint32_t rp_call_count(void) { return rp_calls; }
uint32_t rp_diverged(void) { return diverged_at; }
void rp_set_digest(rp_digest_fn fn) { digest_fn = fn; }

void rp_checkpoint(uint64_t digest)
{
	if (rp_mode == RP_RECORD) {
		if (nchecks == capchecks) {
			capchecks = capchecks ? capchecks * 2 : 256;
			checks = realloc(checks, capchecks * sizeof(Check));
		}
		checks[nchecks].call = rp_calls;
		checks[nchecks].digest = digest;
		nchecks++;
	} else if (rp_mode == RP_REPLAY && !finished) {
		while (check_pos < nchecks && checks[check_pos].call < rp_calls)
			check_pos++;
		if (check_pos < nchecks && checks[check_pos].call == rp_calls) {
			if (checks[check_pos].digest != digest && !diverged_at)
				diverged_at = rp_calls;
			check_pos++;
		}
	}
}

uint32_t rp_checks_compared(void) { return check_pos; }
uint32_t rp_checks_total(void) { return nchecks; }

/* ---- files ---- */
static void put32(FILE *f, uint32_t v)
{
	unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24)};
	fwrite(b, 1, 4, f);
}

static int get32(FILE *f, uint32_t *v)
{
	unsigned char b[4];
	if (fread(b, 1, 4, f) != 4)
		return 0;
	*v = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
	return 1;
}

int rp_flush(void)
{
	char tmp[1100];
	FILE *f;
	uint32_t i, k;
	if (rp_mode != RP_RECORD || !record_path[0])
		return 0;
	snprintf(tmp, sizeof tmp, "%s.tmp", record_path);
	f = fopen(tmp, "wb");
	if (!f)
		return -1;
	fwrite("U6RP", 1, 4, f);
	put32(f, RP_VERSION);
	put32(f, RP_NSTREAMS);
	for (i = 0; i < RP_NSTREAMS; i++) {
		put32(f, streams[i].n);
		for (k = 0; k < streams[i].n; k++) {
			put32(f, streams[i].runs[k].value);
			put32(f, streams[i].runs[k].repeat);
		}
	}
	put32(f, nchecks);
	for (k = 0; k < nchecks; k++) {
		put32(f, checks[k].call);
		put32(f, (uint32_t)checks[k].digest);
		put32(f, (uint32_t)(checks[k].digest >> 32));
	}
	fclose(f);
	remove(record_path);
	return rename(tmp, record_path);
}

int rp_start_record(const char *path)
{
	snprintf(record_path, sizeof record_path, "%s", path);
	rp_mode = RP_RECORD;
	return rp_flush();
}

int rp_start_replay(const char *path)
{
	FILE *f = fopen(path, "rb");
	char magic[4];
	uint32_t ver, ns, i, k, lo, hi;
	if (!f)
		return -1;
	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "U6RP", 4) || !get32(f, &ver) || ver != RP_VERSION || !get32(f, &ns)) {
		fclose(f);
		return -2;
	}
	for (i = 0; i < ns; i++) {
		Stream *s = i < RP_NSTREAMS ? &streams[i] : NULL;
		uint32_t n;
		if (!get32(f, &n))
			goto bad;
		for (k = 0; k < n; k++) {
			Run r;
			if (!get32(f, &r.value) || !get32(f, &r.repeat))
				goto bad;
			if (s) {
				if (s->n == s->cap) {
					s->cap = s->cap ? s->cap * 2 : 256;
					s->runs = realloc(s->runs, s->cap * sizeof(Run));
				}
				s->runs[s->n++] = r;
			}
		}
	}
	if (get32(f, &nchecks)) {
		checks = malloc((nchecks + 1) * sizeof(Check));
		capchecks = nchecks + 1;
		for (k = 0; k < nchecks; k++) {
			if (!get32(f, &checks[k].call) || !get32(f, &lo) || !get32(f, &hi))
				goto bad;
			checks[k].digest = (uint64_t)hi << 32 | lo;
		}
	}
	fclose(f);
	rp_mode = RP_REPLAY;
	return 0;
bad:
	fclose(f);
	return -3;
}

/* Replay ran out: from now on the live inputs are used (the player takes over). */
void rp_take_over(void)
{
	rp_mode = RP_OFF;
}
