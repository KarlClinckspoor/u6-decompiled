/* The port's settings: u6port.cfg in the home directory, one key=value a line. */
#ifndef PORT_SETTINGS_H
#define PORT_SETTINGS_H

typedef struct Settings {
	int scale;              /* 1..8 */
	int fullscreen;
	int aspect;             /* 4:3 */
	int integer_scale;
	int sound;              /* PC speaker effects */
	int volume;             /* 0..100 */
	int recording;          /* black box: record every session */
	int settings_at_start;  /* open the overlay before the game starts */
	char data[1024];        /* the game folder */
} Settings;

extern Settings settings;

void settings_defaults(void);
void settings_load(const char *home);
void settings_save(void);

#endif
