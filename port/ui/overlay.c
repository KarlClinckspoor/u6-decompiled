/* replaces nothing: the settings screen drawn over the game (overlay.h).
 *
 * F11 opens it at any time, F11 or Esc closes it. Up/Down move between rows (past the
 * first row: the tab bar), Left/Right change a value (on the tab bar: the tab), Enter
 * changes a value or runs an action, Tab goes to the next tab. The mouse works too: click
 * a tab or a row. While it is open the game's clock is stopped (plat_clock_pause) and the
 * keys pressed go to the screen, not to the game. Every change is written to the settings
 * file at once.
 */
#include "overlay.h"
#include "settings.h"
#include "../platform/plat.h"

#include <stdio.h>
#include <string.h>

enum { TAB_DISPLAY, TAB_SOUND, TAB_GAME, TAB_REPLAY, NTABS };
static const char *tab_names[NTABS] = {"Display", "Sound", "Game", "Replay"};

static const uint8_t *font;
static int open_now, want_open, tab, row = -1;
static char rec_names[8][64];
static int nrec;
static int confirm_new;	/* the New game row was chosen once: the next Enter does it */

/* colours, found in the game's palette at each draw */
static uint8_t c_black, c_dark, c_mid, c_light, c_white, c_hilite, c_bad;
static uint8_t dim_map[256];
static uint8_t last_pal[768];

static int key_queue[16], kq_head, kq_tail;

static void (*idle_hook)(void);

int overlay_is_open(void) { return open_now; }
void overlay_set_idle_hook(void (*fn)(void)) { idle_hook = fn; }

static int key_hook(uint16_t bios_key, int down)
{
	if (!down)
		return open_now;
	if ((bios_key >> 8) == 0x57) {	/* F11 */
		if (open_now)
			open_now = 0;
		else
			want_open = 1;
		return 1;
	}
	if (!open_now)
		return 0;
	if ((kq_tail + 1) % 16 != kq_head) {
		key_queue[kq_tail] = bios_key;
		kq_tail = (kq_tail + 1) % 16;
	}
	return 1;
}

static int nearest(const uint8_t *pal, int r, int g, int b)
{
	int i, best = 0, bd = 1 << 30;
	for (i = 0; i < 256; i++) {
		int dr = pal[i * 3] - r, dg = pal[i * 3 + 1] - g, db = pal[i * 3 + 2] - b;
		int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
		if (d < bd) {
			bd = d;
			best = i;
		}
	}
	return best;
}

static void prepare_colours(const uint8_t *pal)
{
	int i;
	if (!memcmp(pal, last_pal, 768))
		return;
	memcpy(last_pal, pal, 768);
	c_black = (uint8_t)nearest(pal, 0, 0, 0);
	c_dark = (uint8_t)nearest(pal, 10, 10, 14);
	c_mid = (uint8_t)nearest(pal, 26, 26, 30);
	c_light = (uint8_t)nearest(pal, 44, 44, 46);
	c_white = (uint8_t)nearest(pal, 63, 63, 63);
	c_hilite = (uint8_t)nearest(pal, 63, 52, 10);
	c_bad = (uint8_t)nearest(pal, 63, 16, 16);
	for (i = 0; i < 256; i++)
		dim_map[i] = (uint8_t)nearest(pal, pal[i * 3] / 6, pal[i * 3 + 1] / 6, pal[i * 3 + 2] / 6);
}

static void put_char(uint8_t *fb, int x, int y, unsigned char ch, uint8_t colour)
{
	int r, c;
	if (!font)
		return;
	for (r = 0; r < 8; r++) {
		uint8_t bits = font[ch * 8 + r];
		for (c = 0; c < 8; c++)
			if (bits & (0x80 >> c)) {
				int px = x + c, py = y + r;
				if (px >= 0 && px < 320 && py >= 0 && py < 200)
					fb[py * 320 + px] = colour;
			}
	}
}

static void put_text(uint8_t *fb, int x, int y, const char *s, uint8_t colour, int maxchars)
{
	int i;
	for (i = 0; s[i] && i < maxchars; i++)
		put_char(fb, x + i * 8, y, (unsigned char)s[i], colour);
}

static void fill(uint8_t *fb, int x0, int y0, int w, int h, uint8_t colour)
{
	int x, y;
	for (y = y0; y < y0 + h && y < 200; y++)
		for (x = x0; x < x0 + w && x < 320; x++)
			if (x >= 0 && y >= 0)
				fb[y * 320 + x] = colour;
}

/* ---- the rows of each tab ---- */
typedef struct Row {
	char label[40];
	char value[40];
	const char *help;
	int kind;	/* 0 value, 1 action, 2 text only */
	int id;
} Row;

enum {
	R_FULLSCREEN, R_SCALE, R_ASPECT, R_INTEGER, R_SOUND, R_VOLUME, R_FOLDER, R_RECORDING,
	R_ATSTART, R_NEWGAME, R_STATUS, R_SPEED, R_TAKEOVER, R_REC0
};

static int build_rows(Row *rows)
{
	int n = 0, i;
#define ROW(l, v, h, k, i_) do { snprintf(rows[n].label, 40, "%s", l); snprintf(rows[n].value, 40, "%s", v); \
	rows[n].help = h; rows[n].kind = k; rows[n].id = i_; n++; } while (0)
	char buf[40];
	switch (tab) {
	case TAB_DISPLAY:
		ROW("Fullscreen", settings.fullscreen ? "On" : "Off", "Fullscreen or a window (also Alt+Enter).", 0, R_FULLSCREEN);
		snprintf(buf, sizeof buf, "%dx", settings.scale);
		ROW("Window scale", buf, "The window's size as a multiple of 320x200.", 0, R_SCALE);
		ROW("4:3 aspect", settings.aspect ? "On" : "Off", "Shows the 200 lines as 240, as on a 4:3 CRT; off gives square pixels.", 0, R_ASPECT);
		ROW("Whole-number scaling", settings.integer_scale ? "On" : "Off", "Scales by whole multiples only; off scales freely to fit.", 0, R_INTEGER);
		break;
	case TAB_SOUND:
		ROW("Sound effects", settings.sound ? "On" : "Off", "The PC speaker effects. (Music is not ported yet.)", 0, R_SOUND);
		snprintf(buf, sizeof buf, "%d", settings.volume);
		ROW("Volume", buf, "0 to 100.", 0, R_VOLUME);
		break;
	case TAB_GAME: {
		const char *d = settings.data[0] ? settings.data : "(not set)";
		size_t len = strlen(d);
		char tail[20];
		snprintf(tail, sizeof tail, "%s%s", len > 15 ? ".." : "", len > 15 ? d + len - 13 : d);
		ROW("Game folder", tail, "Your copy of Ultima VI. Change it with --data PATH (next start).", 2, R_FOLDER);
		ROW("Record sessions", settings.recording ? "On" : "Off", "Each session's inputs go to recordings/ in the home folder (newest five kept). Next start.", 0, R_RECORDING);
		ROW("Show this at start", settings.settings_at_start ? "On" : "Off", "Opens this screen before the game starts.", 0, R_ATSTART);
		if (session_can_start_new_game())
			ROW("New game", confirm_new ? "Sure? Enter" : ">", "Starts over from the game folder's saved game (a new character if it has none). Yours goes to backups/.", 1, R_NEWGAME);
		break;
	}
	case TAB_REPLAY:
		ROW(session_status(), "", "Recording: this session is being recorded. Replaying: the game plays a recording back.", 2, R_STATUS);
		if (session_is_replaying()) {
			int s = session_replay_speed();
			if (s)
				snprintf(buf, sizeof buf, "%dx", s);
			else
				snprintf(buf, sizeof buf, "Max");
			ROW("Replay speed", buf, "How fast the recording plays back.", 0, R_SPEED);
			ROW("Take over", ">", "Stop the replay here and play on from this moment.", 1, R_TAKEOVER);
		}
		nrec = session_recordings(rec_names, 5);
		for (i = 0; i < nrec; i++)
			ROW("Replay", rec_names[i], "Restarts the port and plays this recording from its saved game.", 1, R_REC0 + i);
		break;
	}
#undef ROW
	return n;
}

static void change(Row *r, int dir)
{
	static const int speeds[] = {1, 2, 4, 8, 0};
	int i;
	switch (r->id) {
	case R_FULLSCREEN: settings.fullscreen = !settings.fullscreen; break;
	case R_SCALE: settings.scale += dir ? dir : 1; if (settings.scale > 8) settings.scale = 1; if (settings.scale < 1) settings.scale = 8; break;
	case R_ASPECT: settings.aspect = !settings.aspect; break;
	case R_INTEGER: settings.integer_scale = !settings.integer_scale; break;
	case R_SOUND: settings.sound = !settings.sound; break;
	case R_VOLUME: settings.volume += (dir ? dir : 1) * 10; if (settings.volume > 100) settings.volume = dir ? 100 : 0; if (settings.volume < 0) settings.volume = 0; break;
	case R_RECORDING: settings.recording = !settings.recording; break;
	case R_ATSTART: settings.settings_at_start = !settings.settings_at_start; break;
	case R_SPEED:
		for (i = 0; i < 5 && speeds[i] != session_replay_speed(); i++)
			;
		i = (i + (dir < 0 ? 4 : 1)) % 5;
		session_set_replay_speed(speeds[i]);
		return;
	case R_NEWGAME:
		if (!confirm_new) {
			confirm_new = 1;
			return;
		}
		session_new_game();
		return;
	case R_TAKEOVER:
		session_take_over();
		open_now = 0;
		return;
	default:
		if (r->id >= R_REC0) {
			session_play_recording(rec_names[r->id - R_REC0]);
			return;
		}
		return;
	}
	settings_save();
	session_apply_settings();
}

static void draw(uint8_t *fb, uint8_t *pal)
{
	Row rows[16];
	int n, i, x;
	if (!open_now) {
		if (session_is_replaying()) {	/* a small mark while a recording plays */
			prepare_colours(pal);
			put_text(fb, 320 - 7 * 8 - 1, 1, "REPLAY", c_hilite, 6);
		}
		return;
	}
	prepare_colours(pal);
	for (i = 0; i < 320 * 200; i++)
		fb[i] = dim_map[fb[i]];
	fill(fb, 0, 0, 320, 11, c_dark);
	put_text(fb, 4, 2, "Ultima VI port: settings", c_white, 30);
	put_text(fb, 320 - 4 - 9 * 8, 2, "F11 close", c_light, 9);
	x = 4;
	for (i = 0; i < NTABS; i++) {
		int w = (int)strlen(tab_names[i]) * 8 + 8;
		fill(fb, x, 16, w, 12, i == tab ? (row < 0 ? c_hilite : c_mid) : c_dark);
		put_text(fb, x + 4, 18, tab_names[i], i == tab && row < 0 ? c_black : c_white, 12);
		x += w + 4;
	}
	n = build_rows(rows);
	if (row >= n)
		row = n - 1;
	for (i = 0; i < n; i++) {
		int y = 36 + i * 12;
		if (i == row)
			fill(fb, 2, y - 2, 316, 12, c_mid);
		put_text(fb, 8, y, rows[i].label, rows[i].kind == 2 ? c_light : c_white, 22);
		put_text(fb, 8 + 23 * 8, y, rows[i].value, i == row ? c_hilite : c_light, 15);
	}
	if (row >= 0 && row < n && rows[row].help) {	/* the help, wrapped */
		const char *h = rows[row].help;
		int y = 200 - 34, len;
		while (*h && y < 196) {
			len = (int)strlen(h);
			if (len > 38) {
				len = 38;
				while (len > 0 && h[len] != ' ')
					len--;
				if (len == 0)
					len = 38;
			}
			{
				char line[40];
				memcpy(line, h, (size_t)len);
				line[len] = 0;
				put_text(fb, 8, y, line, c_light, 38);
			}
			h += len;
			while (*h == ' ')
				h++;
			y += 10;
		}
	}
}

static void handle_key(int key)
{
	Row rows[16];
	int n = build_rows(rows), scan = key >> 8;
	if (scan != 0x1C)
		confirm_new = 0;
	switch (scan) {
	case 0x01:	/* Esc */
		open_now = 0;
		break;
	case 0x0F:	/* Tab */
		tab = (tab + 1) % NTABS;
		row = -1;
		break;
	case 0x48:	/* up */
		row = row < 0 ? n - 1 : row - 1;
		break;
	case 0x50:	/* down */
		row = row >= n - 1 ? -1 : row + 1;
		break;
	case 0x4B:	/* left */
		if (row < 0) tab = (tab + NTABS - 1) % NTABS;
		else if (rows[row].kind == 0) change(&rows[row], -1);
		break;
	case 0x4D:	/* right */
		if (row < 0) tab = (tab + 1) % NTABS;
		else if (rows[row].kind == 0) change(&rows[row], 1);
		break;
	case 0x1C:	/* Enter */
		if (row >= 0 && rows[row].kind != 2) change(&rows[row], 0);
		else if (row < 0) row = 0;
		break;
	default:
		break;
	}
}

static void handle_click(int16_t mx, int16_t my)
{
	Row rows[16];
	int n = build_rows(rows), i, x = 4;
	if (my >= 16 && my < 28) {
		for (i = 0; i < NTABS; i++) {
			int w = (int)strlen(tab_names[i]) * 8 + 8;
			if (mx >= x && mx < x + w) {
				tab = i;
				row = -1;
			}
			x += w + 4;
		}
		return;
	}
	for (i = 0; i < n; i++) {
		int y = 36 + i * 12;
		if (my >= y - 2 && my < y + 10) {
			row = i;
			if (rows[i].kind != 2)
				change(&rows[i], 0);
		}
	}
}

void overlay_run_modal(void)
{
	uint16_t prev_buttons = 0xffff;
	open_now = 1;
	want_open = 0;
	plat_clock_pause(1);
	plat_video_dirty();
	while (open_now && !plat_quit_requested()) {
		int16_t mx, my;
		uint16_t b;
		plat_pump();
		if (idle_hook)
			idle_hook();
		while (kq_head != kq_tail) {
			handle_key(key_queue[kq_head]);
			kq_head = (kq_head + 1) % 16;
			plat_video_dirty();
		}
		plat_mouse_raw(&mx, &my, &b);
		if (prev_buttons != 0xffff && (b & 1) && !(prev_buttons & 1)) {
			handle_click(mx, my);
			plat_video_dirty();
		}
		prev_buttons = b;
		plat_video_dirty();
		plat_sleep_ms(15);
	}
	open_now = 0;
	plat_clock_pause(0);
	plat_video_dirty();
}

void overlay_pump_hook(void)
{
	if (want_open && !open_now)
		overlay_run_modal();
}

void overlay_init(const uint8_t *f)
{
	font = f;
	plat_set_key_hook(key_hook);
	plat_set_draw_hook(draw);
	plat_set_pump_hook(overlay_pump_hook);
}
