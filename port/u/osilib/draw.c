/* DRAW.ASM: lines and pixels on a surface (1 byte a pixel). Not called by U's C; ported for
 * completeness (docs/port/U_OSILIB.md). */
#include "uosi.h"
#include "ufar.h"

/* D_4592..D_459C: Bresenham's line */
static uint16 stepX, stepY, deltaX, deltaY, count, error;

static void put(struct tSurface *s, uint16 x, uint16 y, int16 color)
{
	*u_pix(s->segment, (uint16)(u_scan(s, (int16)y) + x)) = (uint8_t)color;
}

/* the steps and distances: x1 - x2 (16 bits) and the sign from its borrow, so the
 * coordinates are compared unsigned (a line from a negative x to a positive one goes the
 * other way, with a huge count) */
static void setup(int16 x1, int16 y1, int16 x2, int16 y2)
{
	uint16 ax = (uint16)((uint16)x1 - (uint16)x2), bx = (uint16)((uint16)y1 - (uint16)y2);

	stepX = 0xFFFF;
	if ((uint16)x1 < (uint16)x2) {
		stepX = 1;
		ax = (uint16)-ax;
	}
	stepY = 0xFFFF;
	if ((uint16)y1 < (uint16)y2) {
		stepY = 1;
		bx = (uint16)-bx;
	}
	deltaX = ax;
	deltaY = bx;
}

/* DEC count / JS: on while the count stays positive as a signed word */
static int more(void)
{
	count = (uint16)(count - 1);
	return (int16)count >= 0;
}

/* C_12B7_0004: DRW_line(s, x1, y1, x2, y2, color), clipped to the window */
int DRW_line(struct tSurface *s, int16 x1, int16 y1, int16 x2, int16 y2, int16 color)
{
	int16 x, y;

	if (y1 == y2) {
		/* a horizontal line: REP STOSW/STOSB of the color */
		int16 dx = x1, cx = x2, t;
		uint16 n, di;

		if (dx > cx) {
			t = dx;
			dx = cx;
			cx = t;
		}
		if (cx < s->left)
			return 0;
		if (cx > s->right)
			cx = s->right;
		if (dx > s->right)
			return 0;
		if (dx < s->left)
			dx = s->left;
		if (y1 < s->top || y1 > s->bottom)
			return 0;
		n = (uint16)((uint16)cx - (uint16)dx + 1);
		di = (uint16)(u_scan(s, y1) + (uint16)dx);
		while (n--) {
			*u_pix(s->segment, di) = (uint8_t)color;
			di = (uint16)(di + 1);
		}
		return 0;
	}
	setup(x1, y1, x2, y2);
	x = x1;
	y = y1;
	if (!(deltaX < deltaY)) {
		/* mostly horizontal */
		count = deltaX;
		error = (uint16)(deltaX >> 1);
		for (;;) {
			if (!(x > s->right || x < s->left || y > s->bottom || y < s->top))
				put(s, (uint16)x, (uint16)y, color);
			if (!more())
				break;
			error = (uint16)(error + deltaY);
			if (error > deltaX) {
				error = (uint16)(error - deltaX);
				y = (int16)((uint16)y + stepY);
			}
			x = (int16)((uint16)x + stepX);
		}
	} else {
		/* mostly vertical */
		count = deltaY;
		error = (uint16)(deltaY >> 1);
		for (;;) {
			if (!(x > s->right || x < s->left || y > s->bottom || y < s->top))
				put(s, (uint16)x, (uint16)y, color);
			if (!more())
				break;
			error = (uint16)(error + deltaX);
			if (error > deltaY) {
				error = (uint16)(error - deltaY);
				x = (int16)((uint16)x + stepX);
			}
			y = (int16)((uint16)y + stepY);
		}
	}
	return 0;
}

/* C_12B7_0162: DRW_lineFunc(plot, s, x1, y1, x2, y2, color): the points of the same line,
 * not clipped, each given to plot(s, x, y, color). (In DOS the line goes on from the x
 * and y left on the stack after the call, so a plot that changed its parameters would
 * move the line; a C function cannot.) */
int DRW_lineFunc(void (*plot)(struct tSurface *, int16, int16, int16), struct tSurface *s,
	int16 x1, int16 y1, int16 x2, int16 y2, int16 color)
{
	int16 x = x1, y = y1;

	setup(x1, y1, x2, y2);
	if (!(deltaX < deltaY)) {
		count = deltaX;
		error = (uint16)(deltaX >> 1);
		for (;;) {
			plot(s, x, y, color);
			if (!more())
				break;
			error = (uint16)(error + deltaY);
			if (error > deltaX) {
				error = (uint16)(error - deltaX);
				y = (int16)((uint16)y + stepY);
			}
			x = (int16)((uint16)x + stepX);
		}
	} else {
		count = deltaY;
		error = (uint16)(deltaY >> 1);
		for (;;) {
			plot(s, x, y, color);
			if (!more())
				break;
			error = (uint16)(error + deltaX);
			if (error > deltaY) {
				error = (uint16)(error - deltaY);
				x = (int16)((uint16)x + stepX);
			}
			y = (int16)((uint16)y + stepY);
		}
	}
	return 0;
}

/* C_12B7_022F: the pixel's color, -1 outside the window. The original reads the
 * surface's segment through DS after loading DS with the scanline table's segment
 * (LDS AX,pScanlines ... MOV DS,[BX].segment): right only when the table is in DGROUP,
 * which it is for every surface U passes (screen and its copies: scanlineOfs). The port
 * reads the surface's segment. */
int DRW_getPixel(struct tSurface *s, int16 x, int16 y)
{
	if (y > s->bottom || y < s->top || x < s->left || x > s->right)
		return -1;
	return *u_pix(s->segment, (uint16)(u_scan(s, y) + (uint16)x));
}
