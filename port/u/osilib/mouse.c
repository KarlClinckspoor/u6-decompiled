/* The mouse: MOUSEINI.ASM (initialisation, pointer shape), MOUSESRV.ASM (following it),
 * MOUSECUR.ASM (showing and hiding the pointer, the driver table), MOUSEHIT.ASM (hit
 * tests). docs/port/U_OSILIB.md.
 *
 * DOS: no event handler is installed; the mouse driver is read only by MOUSE_SERVE
 * (int 33h AX=3: x in 640-wide virtual pixels in mode 13h, halved; y; buttons) and once
 * by MOUSE_INIT. Unlike GAME.EXE's MOUSE.ASM, showing and hiding do not nest: MOUSE_SHOW
 * only acts when the pointer is hidden (nMouseSem -1), MOUSE_HIDE only when it is shown
 * (0). The pointer is drawn, saved and restored by the graphics driver's entries 3, 6
 * and 9, through the table main.c fills in (drvDrawPointer...).
 *
 * Port: as GAME's port/sys/mouse.c, MOUSE_SERVE is a poll point (the platform pumped, the
 * timer polled) and reads plat_mouse_state, in the game's 320x200 pixels; the presence
 * of a mouse is plat_mouse_present. The driver entries are ugfx.c's UDRV_3, 6, 9. U's
 * reads are live (not recorded). Before MOUSE_INIT/NOINIT (pMouse 0) the original reads
 * a "tMouse" at DS:0000 (C0M's data); the port's routines return 0 then. */
#include "uosi.h"
#include "ufar.h"
#include "../../platform/plat.h"

struct tMouse *pMouse;     /* D_459E */
int16 nMouseSem;           /* D_45A0 0: the pointer is shown, -1: hidden */
static uint8_t mouse45A2[2];   /* D_45A2 (unused, as in GAME.EXE's MOUSE.ASM) */

/* D_45BC..D_45CC: _drvDrawPointer, drvSavePointer, drvRestPointer, drvPutChar,
 * drvFillRect: 5 far pointers, written by main.c as 10 words (offset, segment). The port
 * calls UDRV_3, 6, 9, 0x12, 0xc directly. */
int16 drvDrawPointer[10];

/* D_45B4..D_45BA: the state at the last change */
static uint16 lastX, lastY, lastL, lastR;

/* C_12DD_005F: the default onMove */
void MOUSE_NOP(void)
{
}

static void reset_struct(struct tMouse *m)
{
	pMouse = m;
	m->buttonL = 0;
	m->buttonR = 0;
	m->x = 0;
	m->y = 0;
	m->onMove = MOUSE_NOP;
	m->installed = 0;
}

/* C_12DD_000A: returns the number of buttons (int 33h BX), 0 without a mouse driver */
int MOUSE_INIT(struct tMouse *m)
{
	(void)mouse45A2;
	reset_struct(m);
	if (!plat_mouse_present())
		return 0;
	pMouse->installed = 1;
	nMouseSem = -1;
	MOUSE_RESET();
	MOUSE_SERVE();
	return 2;
}

/* C_12DD_00A9: the same without asking the mouse driver (no mouse) */
int MOUSE_NOINIT(struct tMouse *m)
{
	reset_struct(m);
	return 0;
}

/* C_12DD_0060: nothing without a mouse or when the shape (table and index) is the same;
 * else hide, change, show. (The original's AX is not meaningful.) */
int MOUSE_POINTER(void *shapes, int16 index)
{
	struct tMouse *m = pMouse;

	if (!m || !m->installed)
		return 0;
	if (index == m->shape && shapes == m->shapes)
		return 0;
	MOUSE_HIDE();
	m = pMouse;
	m->shape = index;
	m->shapes = shapes;
	MOUSE_SHOW();
	return 0;
}

/* C_132C_009A: the next MOUSE_SERVE sees a change */
int MOUSE_RESET(void)
{
	lastX = 0x8001;
	return 0;
}

/* C_132C_0008: when the position or a button changed since the last change: the pointer
 * hidden (if it was shown), the new state stored, onMove called, the pointer shown again;
 * returns 1. Else 0. */
int MOUSE_SERVE(void)
{
	struct tMouse *m = pMouse;
	int16 sx = 0, sy = 0;
	uint16 b = 0, cx, dx, sem;

	if (!m || !m->installed)
		return 0;
	uosi_pump();
	plat_mouse_state(&sx, &sy, &b);
	if (sx < 0) sx = 0;
	if (sx > 319) sx = 319;
	if (sy < 0) sy = 0;
	if (sy > 199) sy = 199;
	cx = (uint16)sx;   /* int 33h: CX = 2x, SHR CX,1 */
	dx = (uint16)sy;
	m = pMouse;
	if (((lastL ^ b) & 1) == 0 && ((lastR ^ b) & 2) == 0 && lastX == cx && lastY == dx) {
		osi_poll_idle(0);
		return 0;
	}
	osi_poll_idle(1);
	sem = (uint16)nMouseSem;
	if (sem == 0)
		MOUSE_HIDE();
	lastX = cx;
	lastY = dx;
	lastL = (uint16)(b & 1);
	lastR = (uint16)(b & 2);
	m = pMouse;
	m->x = (int16)cx;
	m->y = (int16)dx;
	m->buttonL = (char)(b & 1);
	m->buttonR = (char)(b & 2);
	if (m->onMove)
		m->onMove();
	if (sem == 0)
		MOUSE_SHOW();
	return 1;
}

/* C_1339_0000: shows a hidden pointer: what is under it saved, then it is drawn */
int MOUSE_SHOW(void)
{
	if (nMouseSem == -1) {
		nMouseSem++;
		MOUSE_SAVE_BG();
		MOUSE_DRAW();
	}
	return 0;
}

/* C_1339_0016: hides a shown pointer: in mode 13h (the BIOS mode byte) after waiting for
 * the vertical retrace, the background is put back */
int MOUSE_HIDE(void)
{
	if (nMouseSem == 0) {
		nMouseSem--;
		if (uosi_bios_mode == 0x13)
			VID_inRetrace();
		MOUSE_RESTORE_BG();
	}
	return 0;
}

/* C_1339_0038: driver entry 3 */
int MOUSE_DRAW(void)
{
	struct tMouse *m = pMouse;

	if (m && m->installed)
		UDRV_3(m->surface, m->x, m->y, m->shapes, m->shape);
	return 0;
}

/* C_1339_005C: driver entry 6 */
int MOUSE_SAVE_BG(void)
{
	struct tMouse *m = pMouse;

	if (m && m->installed)
		UDRV_6(m->surface, m->saveBuf, m->x, m->y, m->shapes, m->shape);
	return 0;
}

/* C_1339_0086: driver entry 9 */
int MOUSE_RESTORE_BG(void)
{
	struct tMouse *m = pMouse;

	if (m && m->installed)
		UDRV_9(m->surface, m->saveBuf, m->x, m->y, m->shapes, m->shape);
	return 0;
}

/* the header of shape index: shapes + the dword at shapes + 4 + 4 * index (the table read
 * with a 16-bit offset, the sum normalised); its words are the extents right, left, up,
 * down around the hot spot */
static const uint8_t *shape_header(const void *shapes, int16 index)
{
	uint16 bx = (uint16)(((uint16)index << 2) + 4);
	uint32 ofs = (uint32)u_far16(shapes, bx) | ((uint32)u_far16(shapes, (uint16)(bx + 2)) << 16);

	return u_far_add32(shapes, ofs);
}

static int16 hdr(const uint8_t *h, int i)
{
	return (int16)u_far16(h, (uint16)(2 * i));
}

/* C_1344_0000: does the pointer's shape (shown) overlap the window of s? 0 without a
 * mouse or while hidden. Signed compares. */
int MOUSE_IN_AREA(struct tSurface *s)
{
	struct tMouse *m = pMouse;
	const uint8_t *h;

	if (!m || m->installed == 0)
		return 0;
	h = shape_header(m->shapes, m->shape);
	if (nMouseSem < 0)
		return 0;
	if ((int16)(m->x - hdr(h, 1)) > s->right)
		return 0;
	if ((int16)(m->x + hdr(h, 0)) < s->left)
		return 0;
	if ((int16)(m->y - hdr(h, 2)) > s->bottom)
		return 0;
	if ((int16)(m->y + hdr(h, 3)) < s->top)
		return 0;
	return 1;
}

/* C_1344_00A7: is the hot spot in the window of s (and a mouse installed)? */
int MOUSE_IN_RECT(struct tSurface *s)
{
	struct tMouse *m = pMouse;

	if (!m || m->installed == 0)
		return 0;
	if (m->x > s->right || m->x < s->left)
		return 0;
	if (m->y < s->top || m->y > s->bottom)
		return 0;
	return 1;
}

/* C_1344_00DF: is shape index of shapes, its hot spot at (x, y), entirely inside the
 * window of s? */
int SHAPE_IN_RECT(struct tSurface *s, void *shapes, int16 index, int16 x, int16 y)
{
	const uint8_t *h = shape_header(shapes, index);

	if ((int16)(x - hdr(h, 1)) < s->left)
		return 0;
	if ((int16)(x + hdr(h, 0)) > s->right)
		return 0;
	if ((int16)(y - hdr(h, 2)) < s->top)
		return 0;
	if ((int16)(y + hdr(h, 3)) > s->bottom)
		return 0;
	return 1;
}
