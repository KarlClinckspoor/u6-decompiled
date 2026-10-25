/* The session around the game (session.c). */
#ifndef PORT_SESSION_H
#define PORT_SESSION_H

#include <stdint.h>

/* Sets up the files: replay (a recording folder, or a name in recordings/) or record (a
 * folder, or black_box for recordings/<now>). 0 on success. */
int session_start(const char *home_dir, const char *replay, const char *record, int black_box,
	const char *argv0);
void session_end(void);
/* the home before the session starts (the settings screen, character creation) */
void session_set_home(const char *dir);
const char *session_home(void);
/* the directory the game's writes go to and its reads look in first */
const char *session_game_home(void);
void session_pace_tick(uint32_t tick);
void session_exec(const char *path, char **argv);

#endif
