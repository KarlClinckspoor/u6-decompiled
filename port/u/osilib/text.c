/* The text window: TXTPORT.ASM (select), TXTCLR.ASM + CLREOL.ASM + GOTOXY.ASM (clearing,
 * U's clreol and gotoxy), TXTJUST.ASM (strings, word-wrapped and justified), TXTCHAR.ASM
 * (characters), TXTSCRL.ASM (scrolling, clearing a line), TXTPRINT.C (formatted output).
 *
 * A transcription of the assembly: its variables are this file's statics, its registers
 * uint16 locals; offsets into a string or a segment wrap at 64 KB, compares are signed
 * where the original uses JL/JG and unsigned where it uses JB/JA. The characters are drawn
 * by the driver's entry 12h (UDRV_0x12, called with what TXTCHAR.ASM put in the
 * registers), lines cleared by entry 0Ch (UDRV_0xc). docs/port/U_OSILIB.md.
 */
#include "uosi.h"
#include "ufar.h"
#include "../../platform/plat.h"

#include <stdarg.h>
#include <string.h>

/* ---- TXTPORT.ASM ---- */

struct tText *pText;   /* D_45B2 the current text window */

int TXT_select(struct tText *t)
{
	pText = t;
	return 0;
}

/* the font: height (word 0), width of character c at 4 + c */
static uint16 font_height(void)
{
	return u_far16(pText->font, 0);
}

static uint8_t font_width(uint16 c)
{
	return *u_far(pText->font, (uint16)(c + 4));
}

/* ---- TXTSCRL.ASM ---- */

static struct tSurface lineRect;   /* D_45A4 the rectangle TXT_clearLine fills */

/* C_00B1: clear a line of text from (x, y) to the right of the window in the background
 * color: font height + 1 rows (bottom = y + height) */
void TXT_clearLine(int16 x, int16 y)
{
	struct tSurface *w = pText->surface;

	lineRect.segment = w->segment;
	lineRect.pScanlines = w->pScanlines;
	lineRect.top = y;
	lineRect.left = x;
	lineRect.right = w->right;
	lineRect.bottom = (int16)(font_height() + (uint16)lineRect.top);
	UDRV_0xc(&lineRect, (int16)(uint8_t)pText->bgColor);
}

/* C_00F7: copy scanline from of the window to scanline to (left to right): REP MOVSW then
 * MOVSB inside the surface's segment */
static void copy_line(uint16 from, uint16 to)
{
	struct tSurface *w = pText->surface;
	uint16 cx = (uint16)(w->right - w->left + 1);
	uint16 si = (uint16)(u_scan(w, (int16)from) + (uint16)w->left);
	uint16 di = (uint16)(u_scan(w, (int16)to) + (uint16)w->left);
	uint16 n;

	for (n = (uint16)(cx >> 1); n; n--) {
		uint8_t lo = *u_pix(w->segment, si), hi = *u_pix(w->segment, (uint16)(si + 1));
		*u_pix(w->segment, di) = lo;
		*u_pix(w->segment, (uint16)(di + 1)) = hi;
		si = (uint16)(si + 2);
		di = (uint16)(di + 2);
	}
	if (cx & 1)
		*u_pix(w->segment, di) = *u_pix(w->segment, si);
}

/* C_003C: up by one line of text; the line at the bottom is cleared */
void TXT_scrollUp1(void)
{
	struct tSurface *w = pText->surface;
	uint16 h = font_height();
	int16 ax = (int16)((uint16)w->top + h), dx = w->bottom;

	while (ax <= dx) {
		copy_line((uint16)ax, (uint16)((uint16)ax - h));
		ax = (int16)(ax + 1);
	}
	w = pText->surface;
	TXT_clearLine(w->left, (int16)((uint16)w->bottom - font_height() + 1));
}

/* C_0078: down by one line of text; the line at the top is cleared */
void TXT_scrollDown1(void)
{
	struct tSurface *w = pText->surface;
	uint16 h = font_height();
	int16 ax = (int16)((uint16)w->bottom - h), dx = w->top;

	while (ax >= dx) {
		copy_line((uint16)ax, (uint16)((uint16)ax + h));
		if (ax == dx)
			break;
		ax = (int16)(ax - 1);
	}
	w = pText->surface;
	TXT_clearLine(w->left, w->top);
}

/* C_000C, C_0024: n times (0: 65536 times) */
int TXT_scrollUp(int16 n)
{
	uint16 c = (uint16)n;

	do
		TXT_scrollUp1();
	while (--c);
	return 0;
}

int TXT_scrollDown(int16 n)
{
	uint16 c = (uint16)n;

	do
		TXT_scrollDown1();
	while (--c);
	return 0;
}

/* ---- TXTCLR.ASM, CLREOL.ASM, GOTOXY.ASM ---- */

/* C_0034: from the cursor to the end of the window, a line of text at a time: the first
 * from the cursor, the others from the left */
void TXT_clrEos(void)
{
	int16 ax = pText->curY, dx = pText->curX;

	while (ax < pText->surface->bottom) {
		TXT_clearLine(dx, ax);
		dx = pText->surface->left;
		ax = (int16)((uint16)ax + font_height());
	}
}

int TXT_clear(void)
{
	pText->curX = pText->surface->left;
	pText->curY = pText->surface->top;
	TXT_clrEos();
	return 0;
}

int TXT_clearEos(void)
{
	TXT_clrEos();
	return 0;
}

/* clreol() (Turbo C's name, OSI's version): from the cursor to the right of the window */
int clreol(void)
{
	TXT_clearLine(pText->curX, pText->curY);
	return 0;
}

int gotoxy(int16 x, int16 y)
{
	pText->curX = x;
	pText->curY = y;
	return 0;
}

/* ---- TXTCHAR.ASM ---- */

static uint16 theChar;      /* D_45D0 */
static uint16 charWidth;    /* D_45D2 */
static uint16 charHeight;   /* D_45D4 */
static uint16 twiceY;       /* D_45D6 */

int TXT_putc(int16 c)
{
	TXT_putcAL(c);
	return 0;
}

int TXT_bufc(int16 c)
{
	TXT_bufcAL(c);
	return 0;
}

/* C_002E: the character and a 0 after it at the buffer's pointer, which moves on by one */
void TXT_bufcAL(int16 c)
{
	char *p = pText->bufPtr++;

	p[0] = (char)c;
	p[1] = 0;
}

/* C_00B2 */
void TXT_cr(void)
{
	pText->curX = pText->surface->left;
}

/* C_00BF: a new line; at the bottom of the window (when the next line would start at or
 * below the bottom) the window scrolls up instead */
void TXT_newLine(void)
{
	TXT_cr();
	if ((int16)((uint16)pText->curY + font_height()) >= pText->surface->bottom) {
		TXT_scrollUp1();
		return;
	}
	pText->curY = (int16)((uint16)pText->curY + font_height());
}

/* C_0040: the character (AL) at the cursor, 13 a carriage return, 10 a new line; when it
 * does not fit before the right of the window, first a new line. In DOS a character wider
 * than the window loops for ever (new lines, scrolling); the port draws it after the first
 * new line that did not help (logged). */
void TXT_putcAL(int16 c)
{
	uint16 ax = (uint16)(uint8_t)c;
	int retried = 0;
	static int logged;

	if (ax == 0x0D) {
		TXT_cr();
		return;
	}
	if (ax == 0x0A) {
		TXT_newLine();
		return;
	}
	theChar = ax;
	charHeight = font_height();
	charWidth = font_width(ax);
	for (;;) {
		twiceY = (uint16)((uint16)pText->curY << 1);
		if ((int16)((uint16)pText->curX + charWidth - 1) <= pText->surface->right)
			break;
		if (retried && pText->curX == pText->surface->left) {
			if (!logged) {
				logged = 1;
				plat_log("U: TXT_putc: character %u (%u pixels) wider than the window "
					"(the DOS program hangs here)", theChar, charWidth);
			}
			break;
		}
		TXT_newLine();
		retried = 1;
	}
	UDRV_0x12(pText, (int16)theChar, (int16)charHeight, (int16)charWidth, (int16)twiceY);
}

/* ---- TXTJUST.ASM ---- */

static char *pStr;          /* D_457C the string */
static uint16 strPos;       /* D_4580 index of the next line */
static uint16 lineStart;    /* D_4582 index of the line's first character */
static uint16 lineEnd;      /* D_4584 index after the line's last word */
static uint16 lineWidth;    /* D_4586 its width in pixels */
static uint16 lineRoom;     /* D_4588 pixels from the cursor to the right of the window */
static uint8_t spaceAdd;    /* D_458A full justification: pixels added to each space */
static uint8_t spaceRem;    /* D_458B   and one more to the first spaceRem spaces */
static uint16 winRight;     /* D_458C */
static uint16 winLeft;      /* D_458E */
static uint8_t mode;        /* D_4590 the justification */

/* the string's byte at index i (ES:[BX+SI]) */
static uint8_t sb(uint16 i)
{
	return *u_far(pStr, i);
}

int TXT_puts(char *s)
{
	TXT_putsES(s);
	return 0;
}

/* C_001F: the string a line at a time: the first word always goes on the line, then the
 * words while they fit (the end of the string or a 10 ends the line too); the line is
 * printed in the window's justification, then a new line unless it ended with 0 or 10,
 * and the spaces after it are skipped */
void TXT_putsES(char *s)
{
	uint16 si, cx;
	uint8_t al;

	pStr = s;
	strPos = 0;
	mode = (uint8_t)pText->justify;
	winRight = (uint16)pText->surface->right;
	winLeft = (uint16)pText->surface->left;
	for (;;) {
		/* C_004C: the first word */
		si = strPos;
		lineStart = si;
		if (sb(si) != 0) {
			for (;;) {
				al = sb(si);
				si++;
				if (al == 0 || al == 0x0A)
					break;
				if (al == ' ') {
					si--;
					break;
				}
			}
		}
		lineEnd = si;
		/* C_0077: then the words while they fit */
		cx = (uint16)pText->curX;
		si = strPos;
		for (;;) {
			al = sb(si);
			if (al == 0 || al == 0x0A) {
				si++;
				lineEnd = si;
				break;
			}
			if (al == ' ')
				lineEnd = si;
			si++;
			cx = (uint16)(cx + font_width(al));
			if (!(cx < winRight))
				break;
		}
		/* C_00AB: the line's width (lineStart to lineEnd, the 0 or 10 at its end
		 * included) */
		lineRoom = (uint16)(winRight - (uint16)pText->curX + 1);
		cx = 0;
		for (si = lineStart; si < lineEnd; si++)
			cx = (uint16)(cx + font_width(sb(si)));
		lineWidth = cx;
		switch (mode) {
		case 0: TXT_lineLeft(); break;
		case 1: TXT_lineRight(); break;
		case 2: TXT_lineCenter(); break;
		default: TXT_lineFull(); break;
		}
		/* C_0109: the next line */
		si = lineEnd;
		strPos = si;
		al = sb((uint16)(si - 1));
		if (al != 0 && al != 0x0A) {
			TXT_newLine();
			si = strPos;
			while (sb(si) == ' ')
				si++;
			strPos = si;
		}
		if (al == 0)
			return;
	}
}

/* C_0190: the characters from lineStart to lineEnd (a 0 stops) */
void TXT_lineLeft(void)
{
	uint16 si;
	uint8_t al;

	for (si = lineStart; si < lineEnd; si++) {
		al = sb(si);
		if (al == 0)
			break;
		TXT_putcAL(al);
	}
}

/* C_0149: centred when it fits */
void TXT_lineCenter(void)
{
	if (sb(lineStart) != 0 && !(lineWidth > lineRoom))
		pText->curX = (int16)((uint16)pText->curX + (uint16)((uint16)(lineRoom - lineWidth + 1) >> 1));
	TXT_lineLeft();
}

/* C_016C: against the right of the window when it fits, else from its left */
void TXT_lineRight(void)
{
	if (sb(lineStart) == 0 || lineWidth > lineRoom)
		pText->curX = (int16)winLeft;
	else
		pText->curX = (int16)(uint16)(winRight - lineWidth + 1);
	TXT_lineLeft();
}

/* C_01B0: full justification, except the last line of a paragraph (ending with 0 or 10)
 * and a line that does not fit: the leading spaces and the first word as they are, then
 * the room left shared between the spaces (an 8-bit DIV: AL to each, one more to the
 * first AH) */
void TXT_lineFull(void)
{
	uint16 si, dx, cx;
	uint8_t al;

	if (!(lineWidth < lineRoom) || sb((uint16)(lineEnd - 1)) == 0 || sb((uint16)(lineEnd - 1)) == 0x0A) {
		TXT_lineLeft();
		return;
	}
	/* C_01CB: the leading spaces */
	while (sb(lineStart) == ' ') {
		TXT_putcAL(' ');
		lineWidth = (uint16)(lineWidth - font_width(' '));
		lineStart++;
	}
	/* C_01F8: the first word (up to a space: a 0 does not stop it) */
	while ((al = sb(lineStart)) != ' ') {
		TXT_putcAL(al);
		lineWidth = (uint16)(lineWidth - font_width(al));
		lineStart++;
	}
	/* C_0228 */
	dx = (uint16)((uint16)(winRight - (uint16)pText->curX + 1) - lineWidth);
	cx = 0;
	for (si = lineStart; si < lineEnd; si++)
		if (sb(si) == ' ')
			cx++;
	if (cx == 0) {
		TXT_lineLeft();
		return;
	}
	if ((uint8_t)cx == 0 || dx / (uint8_t)cx > 0xFF)
		dos_divide_error("TXT_lineFull");
	spaceRem = (uint8_t)(dx % (uint8_t)cx);
	spaceAdd = (uint8_t)(dx / (uint8_t)cx);
	for (si = lineStart; si < lineEnd; si++) {
		al = sb(si);
		if (al == 0)
			break;
		TXT_putcAL(al);
		if (al == ' ') {
			pText->curX = (int16)((uint16)pText->curX + spaceAdd);
			if (spaceRem) {
				spaceRem--;
				pText->curX = (int16)(pText->curX + 1);
			}
		}
	}
}

/* ---- TXTPRINT.C ---- */

/* Turbo C's itoa, ltoa, ultoa (radix 2..36, lower case digits; itoa and ltoa signed in
 * radix 10 only) and strupr */
static char *u_ultoa(uint32 v, char *buf, int radix)
{
	char tmp[36];
	int n = 0, i = 0;

	do {
		int d = (int)(v % (uint32)radix);
		tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
		v /= (uint32)radix;
	} while (v);
	while (n)
		buf[i++] = tmp[--n];
	buf[i] = 0;
	return buf;
}

static char *u_ltoa(int32 v, char *buf, int radix)
{
	if (radix == 10 && v < 0) {
		buf[0] = '-';
		u_ultoa((uint32)0 - (uint32)v, buf + 1, radix);
		return buf;
	}
	return u_ultoa((uint32)v, buf, radix);
}

static char *u_itoa(int16 v, char *buf, int radix)
{
	if (radix == 10)
		return u_ltoa(v, buf, radix);
	return u_ultoa((uint16)v, buf, radix);
}

static char *u_strupr(char *s)
{
	char *p;

	for (p = s; *p; p++)
		if (*p >= 'a' && *p <= 'z')
			*p = (char)(*p - 'a' + 'A');
	return s;
}

typedef void (*out_fn)(int16);

static void out_putc(int16 c) { TXT_putcAL(c); }
static void out_bufc(int16 c) { TXT_bufcAL(c); }

/* C_12EE_0002 */
static void outstr(out_fn out, char *s)
{
	int16 c;

	while ((c = *s++) != 0)
		out(c);
}

/* C_12EE_0024: %d %u %D (long) %U (unsigned long) %x (hexadecimal, upper case) %c %s, %X %Y
 * the cursor, %F %B the colors, %P prints the window's buffer; another character after %
 * is output as it is (a % at the end outputs the 0 and goes on past it, as in DOS).
 * The arguments: U's int16 arrive promoted to int, its int32 as int32 (int on the port's
 * hosts), its strings as pointers. */
static void format(out_fn out, char *fmt, va_list ap)
{
	int16 c, c2;
	char buf[64];

	while ((c = *fmt++) != 0) {
		if (c != '%') {
			out(c);
			continue;
		}
		c2 = *fmt++;
		switch (c2) {
		case 'd': outstr(out, u_itoa((int16)va_arg(ap, int), buf, 10)); break;
		case 'u': outstr(out, u_ultoa((uint16)va_arg(ap, int), buf, 10)); break;
		case 'D': outstr(out, u_ltoa(va_arg(ap, int32), buf, 10)); break;
		case 'U': outstr(out, u_ultoa(va_arg(ap, uint32), buf, 10)); break;
		case 'x': outstr(out, u_strupr(u_ultoa((uint16)va_arg(ap, int), buf, 16))); break;
		case 'c': out((int16)va_arg(ap, int)); break;
		case 's': outstr(out, va_arg(ap, char *)); break;
		case 'X': pText->curX = (int16)va_arg(ap, int); break;
		case 'Y': pText->curY = (int16)va_arg(ap, int); break;
		case 'B': pText->bgColor = (char)va_arg(ap, int); break;
		case 'F': pText->fgColor = (char)va_arg(ap, int); break;
		case 'P': TXT_puts(pText->buffer); break;
		default: out(c2); break;
		}
	}
}

/* C_12EE_0229: to the window */
int TXT_printf(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	format(out_putc, fmt, ap);
	va_end(ap);
	return 0;
}

/* C_12EE_0249: into the window's buffer (from its start) */
int TXT_sprintf(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pText->bufPtr = pText->buffer;
	format(out_bufc, fmt, ap);
	va_end(ap);
	return 0;
}

/* C_12EE_0277: added to the buffer */
int TXT_sappend(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	format(out_bufc, fmt, ap);
	va_end(ap);
	return 0;
}
