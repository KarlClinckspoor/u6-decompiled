/* replaces U.EXE's character creation (newchar.h).
 *
 * In DOS, ULTIMA6.EXE runs U.EXE (the main menu, the intro, the gypsy and the transfer of
 * a character) and U.EXE runs GAME.EXE. The port runs GAME.EXE's code only, so this screen
 * stands for U.EXE's "create a character": it asks for the name, the sex and the portrait,
 * then lets the player rank the eight virtues in the gypsy's tournament (seven choices,
 * without the gypsy's questions), and writes the result into savegame\objlist exactly as
 * U.EXE does (its routine at 1000:4066 in the GOG U.EXE):
 *
 *   0x0901 STR, 0x0A01 DEX, 0x0B01 INT       15 each, plus the virtues' bonuses
 *   0x13F2 MAGIC                             INT * 2
 *   0x1C71 AvatarFemale                      1 for a female avatar
 *   0x1BF9 KARMA                             75, plus 3 for each choice of Humility
 *   0x0F00 the avatar's name                 14 bytes
 *   0x0C02 experience 370, 0x0E01 HP 90, 0x0FF2 level 3
 *   0x1C72 AvatarPortrait                    portrait + 1 (female 1..6, male 7..12)
 *
 * and the file is written back 0x1C73 bytes long (what GAME.EXE reads).
 * Bonuses for each time a virtue is chosen (U.EXE's tables at DS:07A7, 07B7, 07C7):
 *   Honesty INT+2, Compassion DEX+2, Valor STR+2, Justice DEX+1 INT+1, Sacrifice STR+1
 *   DEX+1, Honor STR+1 INT+1, Spirituality +1 each, Humility none (karma +3).
 */
#include "newchar.h"
#include "../platform/plat.h"
#include "../sys/dosfs.h"
#include "../sys/dosmem.h"
#include "../sys/osi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OBJLIST "SAVEGAME\\OBJLIST"
#define OBJLIST_SIZE 0x1C73
#define OFS_PORTRAIT 0x1C72
#define OFS_FEMALE 0x1C71
#define OFS_KARMA 0x1BF9

static const char *const virtue_names[8] = {
	"Honesty", "Compassion", "Valor", "Justice", "Sacrifice", "Honor", "Spirituality", "Humility"};
static const int8_t bonus_str[8] = {0, 0, 2, 0, 1, 1, 1, 0};
static const int8_t bonus_dex[8] = {0, 2, 0, 1, 1, 0, 1, 0};
static const int8_t bonus_int[8] = {2, 0, 0, 1, 0, 1, 1, 0};

static uint8_t objlist[0x2000];
static uint16_t objlist_len;

static int read_objlist(void)
{
	int16_t h;
	uint16_t got = 0;
	if (dosfs_open(OBJLIST, 0, &h) != 0)
		return -1;
	dosfs_read(h, objlist, sizeof objlist, &got);
	dosfs_close(h);
	objlist_len = got;
	return got >= OBJLIST_SIZE ? 0 : -1;
}

int newchar_needed(void)
{
	if (read_objlist() != 0)
		return 0;	/* no saved game at all: let the game report it */
	return objlist[OFS_PORTRAIT] == 0;
}

void newchar_stats(const int picks[8], int *str, int *dex, int *intel, int *karma)
{
	int v;
	*str = *dex = *intel = 15;
	*karma = 75;
	for (v = 0; v < 8; v++) {
		*str += bonus_str[v] * picks[v];
		*dex += bonus_dex[v] * picks[v];
		*intel += bonus_int[v] * picks[v];
		if (v == 7)
			*karma += 3 * picks[v];
	}
}

int newchar_write(const char *name, int female, int portrait, const int picks[8])
{
	int str, dex, intel, karma;
	int16_t h;
	uint16_t done;
	if (read_objlist() != 0)
		return -1;
	newchar_stats(picks, &str, &dex, &intel, &karma);
	objlist[0x901] = (uint8_t)str;
	objlist[0xA01] = (uint8_t)dex;
	objlist[0xB01] = (uint8_t)intel;
	objlist[0x13F2] = (uint8_t)(intel * 2);
	objlist[OFS_FEMALE] = (uint8_t)(female ? 1 : 0);
	objlist[OFS_KARMA] = (uint8_t)karma;
	memset(objlist + 0xF00, 0, 14);
	strncpy((char *)objlist + 0xF00, name, 13);
	objlist[0xC02] = 370 % 256;
	objlist[0xC03] = 370 / 256;
	objlist[0xE01] = 90;
	objlist[0xFF2] = 3;
	objlist[OFS_PORTRAIT] = (uint8_t)((female ? 0 : 6) + portrait + 1);
	if (dosfs_create(OBJLIST, &h) != 0)
		return -1;
	if (dosfs_write(h, objlist, OBJLIST_SIZE, &done) != 0 || done != OBJLIST_SIZE) {
		dosfs_close(h);
		return -1;
	}
	dosfs_close(h);
	plat_log("new character: %s, %s, portrait %d, STR %d DEX %d INT %d, karma %d", name,
		female ? "female" : "male", portrait + 1, str, dex, intel, karma);
	return 0;
}

/* ---- the screen ---- */

static const uint8_t *font;
static uint8_t screen[320 * 200];
static uint8_t pal[768];
static uint8_t portraits[12][56 * 64];
static int have_portraits;
static uint8_t c_bg, c_panel, c_ink, c_dim, c_hilite, c_white, c_frame;

static int nearest(int r, int g, int b)
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

static void load_palette(void)
{
	int16_t h;
	uint16_t got = 0;
	if (dosfs_open("U6PAL", 0, &h) == 0) {
		dosfs_read(h, pal, 768, &got);
		dosfs_close(h);
	}
	if (got != 768) {	/* a grey ramp */
		int i;
		for (i = 0; i < 768; i++)
			pal[i] = (uint8_t)((i / 3) >> 2);
	}
	c_bg = (uint8_t)nearest(4, 3, 2);
	c_panel = (uint8_t)nearest(60, 55, 40);
	c_frame = (uint8_t)nearest(44, 30, 12);
	c_ink = (uint8_t)nearest(36, 4, 4);
	c_dim = (uint8_t)nearest(40, 34, 24);
	c_hilite = (uint8_t)nearest(63, 52, 10);
	c_white = (uint8_t)nearest(63, 63, 63);
}

static void load_portraits(void)
{
	int16_t fd;
	int i;
	uint8_t *work = dos_farmalloc(0x4000 + 16);
	uint16_t seg;
	if (!work)
		return;
	seg = (uint16_t)(dos_fp_seg(work) + (dos_fp_off(work) + 15) / 16);
	fd = OSI_open("portrait.z");
	if (fd < 0) {
		dos_farfree(work);
		return;
	}
	for (i = 0; i < 12; i++) {
		uint8_t ofs[4];
		uint16_t got = 0;
		dosfs_seek(fd, 0, i * 4, 0);
		dosfs_read(fd, ofs, 4, &got);
		if (got != 4)
			break;
		decompress(fd, (uint32_t)ofs[0] | (uint32_t)ofs[1] << 8 | (uint32_t)ofs[2] << 16 |
			(uint32_t)ofs[3] << 24, portraits[i], seg);
	}
	have_portraits = i == 12;
	OSI_close(fd);
	dos_farfree(work);
}

static void fill(int x0, int y0, int w, int h, uint8_t c)
{
	int x, y;
	for (y = y0; y < y0 + h; y++)
		for (x = x0; x < x0 + w; x++)
			if (x >= 0 && x < 320 && y >= 0 && y < 200)
				screen[y * 320 + x] = c;
}

static void frame(int x0, int y0, int w, int h, uint8_t c)
{
	fill(x0, y0, w, 1, c);
	fill(x0, y0 + h - 1, w, 1, c);
	fill(x0, y0, 1, h, c);
	fill(x0 + w - 1, y0, 1, h, c);
}

static void text(int x, int y, const char *s, uint8_t c)
{
	for (; *s; s++, x += 8) {
		int r, b;
		for (r = 0; r < 8; r++)
			for (b = 0; b < 8; b++)
				if (font && (font[(unsigned char)*s * 8 + r] & (0x80 >> b)) && x + b < 320 && y + r < 200)
					screen[(y + r) * 320 + x + b] = c;
	}
}

static void ctext(int y, const char *s, uint8_t c)
{
	text(160 - (int)strlen(s) * 4, y, s, c);
}

static void portrait(int idx, int x, int y)
{
	int r;
	if (!have_portraits) {
		fill(x, y, 56, 64, c_dim);
		return;
	}
	for (r = 0; r < 64; r++)
		memcpy(screen + (y + r) * 320 + x, portraits[idx] + r * 56, 56);
}

/* a clickable box: returns 1 when (mx,my) is inside */
static int box(int x, int y, int w, int h, const char *label, int selected, int mx, int my)
{
	fill(x, y, w, h, selected ? c_hilite : c_panel);
	frame(x, y, w, h, c_frame);
	text(x + (w - (int)strlen(label) * 8) / 2, y + (h - 8) / 2, label, c_ink);
	return mx >= x && mx < x + w && my >= y && my < y + h;
}

enum { S_NAME, S_SEX, S_PORTRAIT, S_VIRTUES, S_DONE };

typedef struct {
	int step;
	char name[14];
	int female, portrait;
	int bracket[8];		/* the virtues in the tournament's order */
	int round_winners[7];	/* the winner of each of the seven choices */
	int choice;		/* 0..6: the next choice */
	int sel;		/* the focused button of the current step */
	int picks[8];
} State;

static void pair_of(const State *s, int n, int *a, int *b)
{
	/* choices 0..3: the first round; 4, 5: the semifinals; 6: the final */
	if (n < 4) {
		*a = s->bracket[n * 2];
		*b = s->bracket[n * 2 + 1];
	} else if (n < 6) {
		*a = s->round_winners[(n - 4) * 2];
		*b = s->round_winners[(n - 4) * 2 + 1];
	} else {
		*a = s->round_winners[4];
		*b = s->round_winners[5];
	}
}

static void choose(State *s, int which)
{
	int a, b, w;
	pair_of(s, s->choice, &a, &b);
	w = which ? b : a;
	s->round_winners[s->choice] = w;
	s->picks[w]++;
	if (++s->choice == 7)
		s->step = S_DONE;
	s->sel = 0;
}

static void restart(State *s)
{
	int i;
	char keep[14];
	memcpy(keep, s->name, sizeof keep);
	memset(s, 0, sizeof *s);
	memcpy(s->name, keep, sizeof keep);
	for (i = 0; i < 8; i++)
		s->bracket[i] = i;
	for (i = 7; i > 0; i--) {	/* the gypsy shuffles her cards */
		int j = rand() % (i + 1), t = s->bracket[i];
		s->bracket[i] = s->bracket[j];
		s->bracket[j] = t;
	}
}

/* Draws the step; with a click at (mx,my) (or -1), acts on it. */
static void draw(State *s, int mx, int my)
{
	char buf[64];
	int i;
	memset(screen, c_bg, sizeof screen);
	fill(8, 8, 304, 184, c_panel);
	frame(8, 8, 304, 184, c_frame);
	frame(10, 10, 300, 180, c_frame);
	ctext(16, "Ultima VI: a new Avatar", c_ink);
	switch (s->step) {
	case S_NAME:
		ctext(48, "By what name shalt", c_ink);
		ctext(58, "thou be called?", c_ink);
		fill(104, 80, 112, 14, c_white);
		frame(104, 80, 112, 14, c_frame);
		snprintf(buf, sizeof buf, "%s_", s->name);
		text(108, 83, buf, c_ink);
		if (box(120, 110, 80, 16, "Done", 1, mx, my) && s->name[0]) {
			s->step = S_SEX;
			s->sel = 0;
		}
		ctext(170, "Type the name, then Enter.", c_dim);
		break;
	case S_SEX:
		ctext(48, "And art thou", c_ink);
		ctext(58, "Male, or Female?", c_ink);
		if (box(72, 90, 80, 18, "Male", s->sel == 0, mx, my)) {
			s->female = 0;
			s->step = S_PORTRAIT;
			s->sel = 0;
		}
		if (box(168, 90, 80, 18, "Female", s->sel == 1, mx, my)) {
			s->female = 1;
			s->step = S_PORTRAIT;
			s->sel = 0;
		}
		ctext(170, "M or F, or the arrows and Enter.", c_dim);
		break;
	case S_PORTRAIT:
		ctext(30, "Which is thy likeness?", c_ink);
		for (i = 0; i < 6; i++) {
			int x = 62 + (i % 3) * 68, y = 42 + (i / 3) * 68;
			if (i == s->sel)
				fill(x - 3, y - 3, 62, 70, c_hilite);
			portrait((s->female ? 0 : 6) + i, x, y);
			frame(x - 1, y - 1, 58, 66, c_frame);
			if (mx >= x && mx < x + 56 && my >= y && my < y + 64) {
				s->portrait = i;
				s->step = S_VIRTUES;
				s->sel = 0;
			}
		}
		ctext(180, "Arrows and Enter, or a click.", c_dim);
		break;
	case S_VIRTUES: {
		int a, b;
		pair_of(s, s->choice, &a, &b);
		ctext(32, "The gypsy lays down two cards.", c_ink);
		ctext(44, "Which is the greater virtue?", c_ink);
		snprintf(buf, sizeof buf, "Card %d of 7", s->choice + 1);
		ctext(60, buf, c_dim);
		if (box(40, 86, 112, 22, virtue_names[a], s->sel == 0, mx, my))
			choose(s, 0);
		else if (box(168, 86, 112, 22, virtue_names[b], s->sel == 1, mx, my))
			choose(s, 1);
		ctext(130, "Thy choices shape thy strength,", c_dim);
		ctext(140, "dexterity and intelligence.", c_dim);
		break;
	}
	case S_DONE: {
		int str, dex, intel, karma;
		newchar_stats(s->picks, &str, &dex, &intel, &karma);
		portrait((s->female ? 0 : 6) + s->portrait, 40, 40);
		frame(39, 39, 58, 66, c_frame);
		text(112, 42, s->name, c_ink);
		text(112, 56, s->female ? "Female" : "Male", c_ink);
		snprintf(buf, sizeof buf, "Strength     %2d", str);
		text(112, 72, buf, c_ink);
		snprintf(buf, sizeof buf, "Dexterity    %2d", dex);
		text(112, 84, buf, c_ink);
		snprintf(buf, sizeof buf, "Intelligence %2d", intel);
		text(112, 96, buf, c_ink);
		snprintf(buf, sizeof buf, "Thy virtue:  %s", virtue_names[s->round_winners[6]]);
		text(40, 120, buf, c_ink);
		if (box(48, 150, 104, 18, "Begin", s->sel == 0, mx, my))
			s->step = -1;	/* accepted */
		else if (box(168, 150, 104, 18, "Start over", s->sel == 1, mx, my)) {
			restart(s);
		}
		break;
	}
	}
}

static void key(State *s, uint16_t k)
{
	int scan = k >> 8, ch = k & 0xff;
	size_t n;
	switch (s->step) {
	case S_NAME:
		n = strlen(s->name);
		if ((ch == 8 || scan == 0x0E) && n > 0)
			s->name[n - 1] = 0;
		else if (ch == 13 && n > 0)
			s->step = S_SEX;
		else if (ch >= 0x20 && ch < 0x7f && n < 13 && !(n == 0 && ch == ' '))
			s->name[n] = (char)ch, s->name[n + 1] = 0;
		break;
	case S_SEX:
		if (ch == 'm' || ch == 'M' || (ch == 13 && s->sel == 0))
			s->female = 0, s->step = S_PORTRAIT, s->sel = 0;
		else if (ch == 'f' || ch == 'F' || (ch == 13 && s->sel == 1))
			s->female = 1, s->step = S_PORTRAIT, s->sel = 0;
		else if (scan == 0x4B || scan == 0x4D)
			s->sel ^= 1;
		else if (ch == 27)
			s->step = S_NAME;
		break;
	case S_PORTRAIT:
		if (scan == 0x4B) s->sel = (s->sel + 5) % 6;
		else if (scan == 0x4D) s->sel = (s->sel + 1) % 6;
		else if (scan == 0x48 || scan == 0x50) s->sel = (s->sel + 3) % 6;
		else if (ch >= '1' && ch <= '6') s->sel = ch - '1';
		else if (ch == 27) s->step = S_SEX, s->sel = s->female;
		if (ch == 13 || (ch >= '1' && ch <= '6')) {
			s->portrait = s->sel;
			s->step = S_VIRTUES;
			s->sel = 0;
		}
		break;
	case S_VIRTUES:
		if (scan == 0x4B || scan == 0x4D)
			s->sel ^= 1;
		else if (ch == 13)
			choose(s, s->sel);
		else if (ch == '1' || ch == 'a' || ch == 'A')
			choose(s, 0);
		else if (ch == '2' || ch == 'b' || ch == 'B')
			choose(s, 1);
		break;
	case S_DONE:
		if (scan == 0x4B || scan == 0x4D)
			s->sel ^= 1;
		else if (ch == 13 && s->sel == 0)
			s->step = -1;
		else if (ch == 13)
			restart(s);
		break;
	}
}

int newchar_run(const uint8_t *game_font)
{
	State s;
	uint16_t prev = 0xffff;
	font = game_font;
	load_palette();
	load_portraits();
	srand((unsigned)time(NULL));
	memset(&s, 0, sizeof s);
	restart(&s);
	plat_video_source(screen, pal);
	while (s.step >= 0) {
		int16_t mx, my;
		uint16_t b;
		plat_pump();
		if (plat_quit_requested())
			return -1;
		while (plat_key_available())
			key(&s, plat_key_get());
		plat_mouse_raw(&mx, &my, &b);
		if (prev != 0xffff && (b & 1) && !(prev & 1))
			draw(&s, mx, my);
		prev = b;
		if (s.step >= 0)
			draw(&s, -1, -1);
		plat_video_dirty();
		plat_sleep_ms(15);
	}
	memset(screen, 0, sizeof screen);
	plat_video_dirty();
	return newchar_write(s.name, s.female, s.portrait, s.picks);
}

int newchar_quick(const char *spec)
{
	/* NAME[,M|F[,PORTRAIT 1-6[,VIRTUE 1-8]]]: the virtue wins all three of its rounds and
	 * the next virtue in the order takes the other four choices' bonuses as runner-up */
	char name[14];
	int female = 0, portrait = 0, virtue = 2, picks[8] = {0}, i;
	const char *p = strchr(spec, ',');
	size_t n = p ? (size_t)(p - spec) : strlen(spec);
	if (n > 13)
		n = 13;
	memcpy(name, spec, n);
	name[n] = 0;
	if (p) {
		female = p[1] == 'f' || p[1] == 'F';
		p = strchr(p + 1, ',');
	}
	if (p) {
		portrait = atoi(p + 1) - 1;
		if (portrait < 0 || portrait > 5)
			portrait = 0;
		p = strchr(p + 1, ',');
	}
	if (p) {
		virtue = atoi(p + 1) - 1;
		if (virtue < 0 || virtue > 7)
			virtue = 2;
	}
	/* a tournament the virtue wins: it is chosen three times, its last opponent twice,
	 * and two others once */
	picks[virtue] = 3;
	picks[(virtue + 1) % 8] = 2;
	picks[(virtue + 2) % 8] = 1;
	picks[(virtue + 4) % 8] = 1;
	for (i = 0; name[i]; i++)
		;
	if (!i)
		strcpy(name, "Avatar");
	return newchar_write(name, female, portrait, picks);
}
