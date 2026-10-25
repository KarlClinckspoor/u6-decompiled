/* Record and replay.
 *
 * Every value the game reads from the outside world goes through rp_value(): the keys,
 * the mouse, the timer ticks, the random seed, the date. Recording appends each value to
 * its stream; replaying returns the recorded values in order instead of the live ones, so
 * the game takes exactly the same decisions however fast the host is. The streams are
 * separate so that one kind of read (say, the mouse) cannot shift another.
 *
 * Off (neither recording nor replaying) rp_value returns the live value and costs a test.
 * When a replay's stream runs out, rp_finished() becomes 1 and the live value is used.
 */
#ifndef REPLAY_H
#define REPLAY_H

#include <stdint.h>

enum {
	RP_TICKS = 0,    /* the 18.2 Hz timer tick counter, each time the game reads it */
	RP_KEY,          /* each result of a keyboard poll (0 = no key) */
	RP_SHIFT,        /* BIOS shift flags */
	RP_MOUSE_X,
	RP_MOUSE_Y,
	RP_MOUSE_BTN,
	RP_SEED,         /* the random generator's seed from the clock */
	RP_DATE,         /* DOS date/time reads */
	RP_MISC,
	RP_NSTREAMS
};

enum { RP_OFF = 0, RP_RECORD, RP_REPLAY };

/* a digest of the game's state is taken every RP_CHECK_EVERY input reads */
#define RP_CHECK_EVERY 4096

extern int rp_mode;

/* Returns the live value (off, recording: and records it) or the recorded one (replay). */
uint32_t rp_value_(int stream, uint32_t live);
#define rp_value(stream, live) (rp_mode == RP_OFF ? (uint32_t)(live) : rp_value_((stream), (live)))

int rp_start_record(const char *path);   /* RECORD.U6R to write */
int rp_start_replay(const char *path);   /* 0 ok, <0 unreadable */
int rp_flush(void);                      /* write the recording out (black box) */
void rp_take_over(void);                 /* stop replaying: live input from now on */

/* 1 while replaying and a stream has run out: the replay is over. */
int rp_finished(void);
uint32_t rp_call_count(void);

typedef uint64_t (*rp_digest_fn)(void);
void rp_set_digest(rp_digest_fn fn);
void rp_checkpoint(uint64_t digest);
uint32_t rp_diverged(void);              /* call count of the first differing checkpoint, 0 none */
uint32_t rp_checks_compared(void);
uint32_t rp_checks_total(void);

#endif
