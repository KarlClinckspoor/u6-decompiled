/* MOUSE.ASM: the mouse pointer.
 *
 * DOS: the library never installs an int 33h event handler. The mouse driver's state is
 * read only by MOUSE_SERVE (int 33h AX=3: position in 640x200 virtual pixels in mode 13h,
 * halved for x; buttons in BX), and MOUSE_SERVE is only called by the game's code (its
 * input loops, the disk prompt). When the position or a button changed, MOUSE_SERVE hides
 * the pointer, stores the new state in the game's struct tMouse, calls Mouse.onMove
 * (CON_UpdateMousePointer, which picks the pointer shape), and shows the pointer again.
 * So the mouse state and the callback already change only at points fixed by the game's
 * own code, and the port keeps exactly those points: there is no other.
 *
 * Port: each MOUSE_SERVE of an installed mouse is a poll point: plat_pump(), then x, y
 * and the buttons are read through rp_value (RP_MOUSE_X, RP_MOUSE_Y, RP_MOUSE_BTN), in
 * the game's 320x200 pixels. Whether a mouse is present (int 33h AX=0) goes through
 * rp_value(RP_MISC) in MOUSE_INIT.
 *
 * Showing and hiding nest with a counter (_nMouseSem): -1 hidden, 0 shown. Showing
 * saves the background under the pointer (driver entry 0Ch) and draws it (30h); hiding
 * waits for the vertical retrace (port 3DAh bit 3) and puts the background back (0Fh).
 * The port has no retrace to wait for: the screen is presented by plat_pump, so the wait
 * is left out.
 *
 * The driver entries are port/gfx/mcga.c's GRDRV_0C/0F/30. MOUSE.ASM pushed the surface's
 * near pointer and the far pointers saveBuf and shapes as words; they take the pointers.
 */
#include "osi.h"
#include "dosmem.h"
#include "../gfx/mcga.h"   /* GRDRV_0C, GRDRV_0F, GRDRV_30 */
#include "../platform/plat.h"
#include "../replay/replay.h"

static struct tMouse *pMouse;     /* _pMouse, D_2FB4 */
static int16_t nMouseSem;         /* _nMouseSem, D_2FB6 */
static uint16_t lastX;            /* D_2FBA */
static uint16_t lastY;            /* D_2FBC */
static uint16_t lastL;            /* D_2FBE */
static uint16_t lastR;            /* D_2FC0 */

/* _MOUSE_0055 */
static void mouse_dummy(void)
{
}

static uint16_t rd16(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

/* The pointer shape header: shapes + the 32-bit offset at shapes + 4 + index*4. Its words
 * are the extents of the shape around the hot spot: [0] right, [1] left, [2] up, [3] down. */
static const uint8_t *shape_header(const struct tMouse *m)
{
	const uint8_t *base = (const uint8_t *)m->shapes;
	const uint8_t *e = base + (uint16_t)((m->shapeIndex << 2) + 4);
	uint32_t off = (uint32_t)rd16(e) | ((uint32_t)rd16(e + 2) << 16);
	return base + off;
}

int MOUSE_INIT(struct tMouse *mouse)
{
	uint32_t present;

	pMouse = mouse;
	mouse->buttonL = 0;
	mouse->buttonR = 0;
	mouse->posX = 0;
	mouse->posY = 0;
	mouse->onMove = mouse_dummy;
	mouse->installed = 0;
	/* int 33h AX=0: reset, AX = FFFFh if a driver is there, BX = number of buttons */
	present = rp_value(RP_MISC, plat_mouse_present() ? 1u : 0u);
	if (!present)
		return 0;
	mouse->installed = 1;
	nMouseSem = -1;
	lastX = 0x8001;   /* _MOUSE_0124: force the next MOUSE_SERVE to see a change */
	MOUSE_SERVE();
	return 2;
}

int MOUSE_POINTER(void *shapes, uint16_t shapeIndex)
{
	struct tMouse *m = pMouse;

	if (!m || !m->installed)
		return 0;
	if (shapeIndex == m->shapeIndex && shapes == m->shapes)
		return 0;
	MOUSE_HIDE();
	m->shapeIndex = shapeIndex;
	m->shapes = shapes;
	MOUSE_SHOW();
	return 0;
}

int MOUSE_SERVE(void)
{
	struct tMouse *m = pMouse;
	int16_t lx = 0, ly = 0;
	uint16_t lb = 0, x, y, b;

	if (!m || !m->installed)
		return 0;
	plat_pump();
	if (!osi_replaying()) {
		plat_mouse_state(&lx, &ly, &lb);
		if (lx < 0)
			lx = 0;
		if (lx > 319)
			lx = 319;
		if (ly < 0)
			ly = 0;
		if (ly > 199)
			ly = 199;
	}
	x = (uint16_t)rp_value(RP_MOUSE_X, (uint16_t)lx);
	y = (uint16_t)rp_value(RP_MOUSE_Y, (uint16_t)ly);
	b = (uint16_t)rp_value(RP_MOUSE_BTN, lb);
	if (((lastL ^ b) & 1) == 0 && ((lastR ^ b) & 2) == 0 && lastX == x && lastY == y) {
		osi_poll_idle(0);
		return 0;
	}
	osi_poll_idle(1);
	MOUSE_HIDE();
	lastX = x;
	lastY = y;
	lastL = b & 1;
	lastR = b & 2;
	m->posX = (int16_t)x;
	m->posY = (int16_t)y;
	m->buttonL = (unsigned char)(b & 1);
	m->buttonR = (unsigned char)(b & 2);
	if (m->onMove)
		m->onMove();
	MOUSE_SHOW();
	return 1;
}

int MOUSE_SHOW(void)
{
	if (++nMouseSem == 0) {
		MOUSE_SAVE_BG();
		MOUSE_DRAW();
	}
	return 0;
}

/* _MOUSE_01B0: driver entry 0Fh */
static void mouse_restore_bg(void)
{
	struct tMouse *m = pMouse;

	if (!m || !m->installed)
		return;
	GRDRV_0F(m->surface, m->saveBuf, m->posX, m->posY, m->shapes, m->shapeIndex);
}

int MOUSE_HIDE(void)
{
	if (--nMouseSem == -1) {
		/* _MOUSE_01DD waited for the vertical retrace here */
		mouse_restore_bg();
	}
	return 0;
}

int MOUSE_DRAW(void)
{
	struct tMouse *m = pMouse;

	if (!m || !m->installed)
		return 0;
	GRDRV_30(m->surface, m->posX, m->posY, m->shapes, m->shapeIndex);
	return 0;
}

int MOUSE_SAVE_BG(void)
{
	struct tMouse *m = pMouse;

	if (!m || !m->installed)
		return 0;
	GRDRV_0C(m->surface, m->saveBuf, m->posX, m->posY, m->shapes, m->shapeIndex);
	return 0;
}

/* Does the pointer shape (shown) overlap the rectangle of area? Signed compares. */
int16_t MOUSE_IN_AREA(struct tSurface *area)
{
	struct tMouse *m = pMouse;
	const uint8_t *h;

	if (!m || !m->installed)
		return 0;
	h = shape_header(m);
	if (nMouseSem < 0)
		return 0;
	if ((int16_t)(m->posX - (int16_t)rd16(h + 2)) > (int16_t)area->right)
		return 0;
	if ((int16_t)(m->posX + (int16_t)rd16(h)) < (int16_t)area->left)
		return 0;
	if ((int16_t)(m->posY - (int16_t)rd16(h + 4)) > (int16_t)area->bottom)
		return 0;
	if ((int16_t)(m->posY + (int16_t)rd16(h + 6)) < (int16_t)area->top)
		return 0;
	return 1;
}

/* _MOUSE_0297 (uncalled): is the hot spot inside area? */
int16_t MOUSE_0297(struct tSurface *area)
{
	struct tMouse *m = pMouse;

	if (!m || !m->installed)
		return 0;
	if (m->posX > (int16_t)area->right || m->posX < (int16_t)area->left)
		return 0;
	if (m->posY < (int16_t)area->top || m->posY > (int16_t)area->bottom)
		return 0;
	return 1;
}

/* _MOUSE_02CF (uncalled): is shape shapeIndex at (x, y) entirely inside area? */
int16_t MOUSE_02CF(struct tSurface *area, void *shapes, uint16_t shapeIndex, int16_t x, int16_t y)
{
	const uint8_t *base = (const uint8_t *)shapes;
	const uint8_t *e = base + (uint16_t)((shapeIndex << 2) + 4);
	const uint8_t *h = base + ((uint32_t)rd16(e) | ((uint32_t)rd16(e + 2) << 16));

	if ((int16_t)(x - (int16_t)rd16(h + 2)) < (int16_t)area->left)
		return 0;
	if ((int16_t)(x + (int16_t)rd16(h)) > (int16_t)area->right)
		return 0;
	if ((int16_t)(y - (int16_t)rd16(h + 4)) < (int16_t)area->top)
		return 0;
	if ((int16_t)(y + (int16_t)rd16(h + 6)) > (int16_t)area->bottom)
		return 0;
	return 1;
}
