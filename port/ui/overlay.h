/* The settings screen over the game (F11), as in Underworld Exhumed's ports: tabs of
 * settings, saved at once to u6port.cfg; the game is paused while it is open. */
#ifndef PORT_OVERLAY_H
#define PORT_OVERLAY_H

#include <stdint.h>

/* font: 256 characters of 8x8 pixels, 8 bytes each, top row first, bit 7 leftmost (U6.CH) */
void overlay_init(const uint8_t *font);
/* Opens the screen now and returns when it is closed (the game waits). */
void overlay_run_modal(void);
int overlay_is_open(void);
/* called on each round of the open screen's loop (the port's test options) */
void overlay_set_idle_hook(void (*fn)(void));

/* what the overlay asks of the session (port/sys/session.c) */
int session_is_replaying(void);
int session_is_recording(void);
const char *session_status(void);            /* one line about the recording/replay */
int session_recordings(char names[][64], int max);  /* newest first */
void session_play_recording(const char *name);      /* restarts the port to replay it */
void session_take_over(void);
int session_replay_speed(void);              /* 0 = as fast as possible, else 1, 2, 4, 8 */
void session_set_replay_speed(int speed);
void session_apply_settings(void);
int session_can_start_new_game(void);
void session_new_game(void);                 /* restarts the port */           /* display and sound settings changed */

#endif
