/* A character without U.EXE's screens, for tests (--new-avatar): newchar.h.
 *
 * The port runs U.EXE itself (port/u/urun.c) for the menu, the introduction and the
 * gypsy; this writes what U.EXE's WriteNewCharacter writes (SRC_U/chargen.c) for a given
 * name, sex, portrait and choice of virtues, into savegame\objlist:
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
