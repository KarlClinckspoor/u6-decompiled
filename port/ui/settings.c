/* replaces nothing: the port's settings file (settings.h). */
#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Settings settings;
static char cfg_path[1100];

void settings_defaults(void)
{
	memset(&settings, 0, sizeof settings);
	settings.scale = 3;
	settings.aspect = 1;
	settings.integer_scale = 1;
	settings.sound = 1;
	settings.volume = 50;
	settings.recording = 1;
	settings.settings_at_start = 0;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int settings_load(const char *home)
{
	FILE *f;
	char line[1200];
	snprintf(cfg_path, sizeof cfg_path, "%s%su6port.cfg", home,
		home[0] && home[strlen(home) - 1] != '/' && home[strlen(home) - 1] != '\\' ? "/" : "");
	f = fopen(cfg_path, "r");
	if (!f)
		return 0;
	while (fgets(line, sizeof line, f)) {
		char *eq = strchr(line, '='), *v, *e;
		if (!eq || line[0] == '#')
			continue;
		*eq = 0;
		v = eq + 1;
		e = v + strlen(v);
		while (e > v && (e[-1] == '\n' || e[-1] == '\r'))
			*--e = 0;
		if (!strcmp(line, "scale")) settings.scale = clampi(atoi(v), 1, 8);
		else if (!strcmp(line, "fullscreen")) settings.fullscreen = atoi(v) != 0;
		else if (!strcmp(line, "aspect")) settings.aspect = atoi(v) != 0;
		else if (!strcmp(line, "integer")) settings.integer_scale = atoi(v) != 0;
		else if (!strcmp(line, "sound")) settings.sound = atoi(v) != 0;
		else if (!strcmp(line, "volume")) settings.volume = clampi(atoi(v), 0, 100);
		else if (!strcmp(line, "recording")) settings.recording = atoi(v) != 0;
		else if (!strcmp(line, "settings-at-start")) settings.settings_at_start = atoi(v) != 0;
		else if (!strcmp(line, "data")) snprintf(settings.data, sizeof settings.data, "%s", v);
	}
	fclose(f);
	return 1;
}

void settings_save(void)
{
	FILE *f;
	if (!cfg_path[0])
		return;
	f = fopen(cfg_path, "w");
	if (!f)
		return;
	fprintf(f, "# Ultima VI port settings (F11 in the game)\n");
	fprintf(f, "scale=%d\nfullscreen=%d\naspect=%d\ninteger=%d\nsound=%d\nvolume=%d\nrecording=%d\n"
		"settings-at-start=%d\ndata=%s\n", settings.scale, settings.fullscreen, settings.aspect,
		settings.integer_scale, settings.sound, settings.volume, settings.recording,
		settings.settings_at_start, settings.data);
	fclose(f);
}
