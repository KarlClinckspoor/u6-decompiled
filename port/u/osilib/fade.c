/* FADE.C: palette fades (docs/port/U_OSILIB.md). The colors of a picture are found by
 * reading its pixels (the inline assembly of FADE_in/FADE_out), then moved a step at each
 * vertical retrace by FADE_step, which U's moongate also uses to move shapes. */
#include "uosi.h"
#include "ufar.h"

#include <stdlib.h>

struct tRGB { int16 r, g, b; };

int16 fadeStart = 1;   /* D_45E2 1: the next FADE_step starts a fade */
/* LIBBSS.ASM's FADE.C variables */
int16 *fadeErr;        /* D_681C FADE_step: the error term of each value */
int16 *fadeDist;       /* D_681E   its distance to go */
int16 *fadeDir;        /* D_6820   its direction (+1/-1) */
int16 fadeSteps;       /* D_6822   the largest distance: the number of steps */
int16 fadeLeft;        /* D_6824   the steps left */
char *fadeInColors;    /* D_6826 FADE_in: the colors the picture uses */
char *fadeOutColors;   /* D_6828 FADE_out: the same */

/* C_136D_0004: each of the n values of cur one step towards target, all arriving in the
 * same number of steps (as the points of Bresenham's lines); 0 once they have arrived */
int FADE_step(int16 *cur, int16 *target, int16 n)
{
	int16 i, d;

	if (fadeStart) {
		fadeErr = (int16 *)malloc((size_t)(uint16)(n * 2));
		fadeDist = (int16 *)malloc((size_t)(uint16)(n * 2));
		fadeDir = (int16 *)malloc((size_t)(uint16)(n * 2));
		fadeSteps = 0;
		for (i = 0; i < n; i++) {
			d = (int16)(cur[i] - target[i]);
			if (d < 0) {
				d = (int16)-d;
				fadeDir[i] = 1;
			} else {
				fadeDir[i] = -1;
			}
			fadeDist[i] = d;
			if (d > fadeSteps)
				fadeSteps = d;
		}
		d = (int16)(fadeSteps / 2);
		for (i = 0; i < n; i++)
			fadeErr[i] = d;
		fadeLeft = fadeSteps;
		fadeStart = 0;
	}
	if (fadeLeft-- == 0) {
		free(fadeErr);
		free(fadeDist);
		free(fadeDir);
		fadeStart = 1;
		return 0;
	}
	for (i = 0; i < n; i++) {
		if ((fadeErr[i] = (int16)(fadeErr[i] + fadeDist[i])) > fadeSteps) {
			fadeErr[i] = (int16)(fadeErr[i] - fadeSteps);
			cur[i] = (int16)(cur[i] + fadeDir[i]);
		}
	}
	return 1;
}

/* the inline assembly: mark the colors of the pixels in the window of s (rows top..,
 * width bytes from left each; a width or height of 0 is 65536, as LOOP and DEC/JNZ count) */
static void used_colors(struct tSurface *s, char *used)
{
	uint16 width = (uint16)(s->right - s->left + 1);
	uint16 height = (uint16)(s->bottom - s->top + 1);
	uint16 row = 0;

	do {
		uint16 si = (uint16)(u_scan(s, (int16)(uint16)((uint16)s->top + row)) + (uint16)s->left);
		uint16 cx = width;
		do {
			used[*u_pix(s->segment, si)] = 1;
			si = (uint16)(si + 1);
		} while (--cx);
		row++;
	} while (--height);
}

/* the list of the colors marked */
static int16 color_list(char *used, int16 *list)
{
	int16 count = 0, i;

	for (i = 0; i < 256; i++)
		if (used[i])
			list[count++] = i;
	return count;
}

/* C_136D_014D: the picture src shown on dst (driver entry 0Fh), its colors fading in from
 * the color of dst's top left pixel */
int FADE_in(struct tSurface *src, struct tSurface *dst)
{
	int16 count, i;
	int16 *list;
	struct tRGB *cur, *target, rgb;

	list = (int16 *)calloc(256, 2);
	fadeInColors = (char *)calloc(256, 1);
	used_colors(src, fadeInColors);
	count = color_list(fadeInColors, list);
	free(fadeInColors);
	cur = (struct tRGB *)calloc((size_t)(uint16)count, sizeof(struct tRGB));
	target = (struct tRGB *)calloc((size_t)(uint16)count, sizeof(struct tRGB));
	PAL_get((int16)DRW_getPixel(dst, dst->left, dst->top), &rgb.r);
	VID_waitRetrace();
	for (i = 0; i < count; i++) {
		PAL_get(list[i], &cur[i].r);
		PAL_set(list[i], &rgb.r);
		target[i] = rgb;
	}
	UDRV_0xf(src, dst);
	while (FADE_step(&target[0].r, &cur[0].r, (int16)(count * 3))) {
		VID_waitRetrace();
		for (i = 0; i < count; i++)
			PAL_set(list[i], &target[i].r);
	}
	free(target);
	free(cur);
	free(list);
	return 0;
}

/* C_136D_0346: the colors used in the window of s fade to the palette's color "color".
 * The original reads s's scanline table as a near pointer (DS:offset): right for every
 * surface U passes (screen, textArea: scanlineOfs, in DGROUP); the port reads the table
 * where it is. */
int FADE_out(struct tSurface *s, int16 color)
{
	int16 count, i;
	int16 *list;
	struct tRGB *cur, *target, rgb;

	list = (int16 *)calloc(256, 2);
	fadeOutColors = (char *)calloc(256, 1);
	used_colors(s, fadeOutColors);
	count = color_list(fadeOutColors, list);
	free(fadeOutColors);
	cur = (struct tRGB *)calloc((size_t)(uint16)count, sizeof(struct tRGB));
	target = (struct tRGB *)calloc((size_t)(uint16)count, sizeof(struct tRGB));
	PAL_get(color, &rgb.r);
	VID_waitRetrace();
	for (i = 0; i < count; i++) {
		PAL_get(list[i], &cur[i].r);
		target[i] = rgb;
	}
	while (FADE_step(&cur[0].r, &target[0].r, (int16)(count * 3))) {
		VID_waitRetrace();
		for (i = 0; i < count; i++)
			PAL_set(list[i], &cur[i].r);
	}
	free(target);
	free(cur);
	free(list);
	return 0;
}
