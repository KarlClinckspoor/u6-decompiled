/* The session around the game (session.c). */
#ifndef PORT_SESSION_H
#define PORT_SESSION_H

#include <stdint.h>

/* Sets up the files and Exhume's replay.c for the game: replay (a recording folder, or a name
 * in recordings/; with replay_out, a folder the replay runs in and writes its dumps to, else a
 * temporary one and no dumps), or record (a folder, with the dumps), or black_box (to
 * recordings/<now>), or none of them. 0 on success. */
int session_start(const char *home_dir, const char *replay, const char *replay_out,
	const char *record, int black_box, const char *argv0);
/* --dos-replay: FILE (a RECORD.OUT, DOS's or the port's) replayed from the saved game in the
 * home folder, the dumps written there (tools/replay.py's run_port, tools/rp.py port) */
int session_start_file(const char *home_dir, const char *file);
void session_end(void);
/* the home before the session starts (the settings screen, character creation) */
void session_set_home(const char *dir);
const char *session_home(void);
/* the directory the game's writes go to and its reads look in first */
const char *session_game_home(void);
void session_exec(const char *path, char **argv);
/* restarts the port with U.EXE (the main menu) */
void session_restart_menu(void);

#endif
