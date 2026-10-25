/* VMODE.ASM (the BIOS video mode), VSYNC.ASM (the vertical retrace), PALETTE.ASM (the VGA
 * DAC), and the port's presentation and waits (docs/port/U_OSILIB.md).
 *
 * The screen: A000:0000 in the DOS arena (what ugfx.c draws on), shown with the DAC of
 * port/gfx/vgadac.h (port/u/urun.c makes them the platform's video source when U sets mode
 * 13h). ugfx.c does not tell the platform when it draws, so every poll point
 * and wait of the library marks the screen changed (plat_video_dirty) and pumps: the
 * platform presents at most once per host frame.
 *
 * The retrace: VGA mode 13h refreshes at 25.175 MHz / (800 * 449) = 70.086 Hz; the port
 * counts frames on plat_ms() (frame f starts at f / 70.086 s). Port 3DAh's bit 3 (the
 * vertical retrace) is on for the 2 lines of the sync pulse, 64 microseconds of a frame:
 *   VID_waitRetrace (wait for the end of the retrace, then for the next one) waits until
 *     the next frame starts;
 *   VID_inRetrace (wait until in the retrace) waits until the next frame starts too, unless
 *     the display is still in the retrace a VID_waitRetrace just ended in: in DOS, code
 *     that does little between the two (GetKey: VID_waitRetrace, MOUSE_SERVE, which hides
 *     the pointer: VID_inRetrace) does not lose a frame. The port gives that once, in the
 *     same millisecond of the same frame (the clock has 1 ms steps).
 * The waits are on the frame grid, not "14.3 ms from now", so a loop paced by them runs
 * at 70.086 Hz as long as one pass takes less than a frame.
 */
#include "uosi.h"
#include "ufar.h"
#include "../../platform/plat.h"
#include "../../gfx/vgadac.h"
#include "../../sys/dosfs.h"

#include <string.h>

int16 uosi_bios_mode = 0x13;   /* 0040:0049 */

void (*uosi_wait_hook)(int kind, uint64_t arg);

/* ---- presenting ---- */

/* the platform's video source (A000:0000 and vga_palette) is set by port/u/urun.c's mode
 * hook when U sets mode 13h (DRV(0)) */
void uosi_present(void)
{
	plat_video_dirty();
}

void uosi_pump(void)
{
	uosi_present();
	plat_pump();
	uosi_poll();
}

/* ---- the frame clock ---- */

#define VGA_MHZ_X1000 70086u   /* frames per 1000 s */

static uint64_t frame_of(uint32_t ms)
{
	return (uint64_t)ms * VGA_MHZ_X1000 / 1000000u;
}

static uint32_t frame_start(uint64_t f)
{
	return (uint32_t)((f * 1000000u + VGA_MHZ_X1000 - 1) / VGA_MHZ_X1000);
}

/* the retrace a VID_waitRetrace ended in: its frame, the ms it ended at */
static int credit;
static uint64_t credit_frame;
static uint32_t credit_ms;

/* until frame f starts: pump and poll, sleep until 1 ms before, then poll the last ms */
static void wait_until(uint32_t due)
{
	uint32_t now;

	uosi_present();
	for (;;) {
		plat_pump();
		uosi_poll();
		now = plat_ms();
		if ((int32_t)(now - due) >= 0)
			break;
		if (due - now > 1)
			plat_sleep_ms(due - now - 1);
	}
	osi_poll_idle(1);
}

/* to the start of the next frame; returns it */
static uint64_t next_frame(void)
{
	uint64_t f = frame_of(plat_ms()) + 1;

	wait_until(frame_start(f));
	return f;
}

void uosi_wait_frame(void)
{
	if (uosi_wait_hook) {
		uosi_wait_hook(UOSI_WAIT_RETRACE, 0);
		return;
	}
	next_frame();
}

/* C_1368_0015 */
int VID_waitRetrace(void)
{
	if (uosi_wait_hook) {
		credit = 1;
		uosi_wait_hook(UOSI_WAIT_RETRACE, 0);
		return 0;
	}
	credit_frame = next_frame();
	credit_ms = plat_ms();
	credit = 1;
	return 0;
}

static int in_retrace_now(void)
{
	uint32_t now;

	if (!credit)
		return 0;
	credit = 0;
	if (uosi_wait_hook)
		return 1;
	now = plat_ms();
	return now == credit_ms && frame_of(now) == credit_frame;
}

/* C_1368_0002 */
int VID_inRetrace(void)
{
	if (in_retrace_now())
		return 0;
	if (uosi_wait_hook) {
		uosi_wait_hook(UOSI_WAIT_INRETRACE, 0);
		return 0;
	}
	next_frame();
	return 0;
}

/* C_1368_002D: VID_inRetrace when the pointer is shown */
int VID_mouseRetrace(void)
{
	if (pMouse && pMouse->installed && nMouseSem >= 0)
		VID_inRetrace();
	return 0;
}

/* C_1368_0040: 8 in the retrace, else 0 */
int VID_retrace(void)
{
	return in_retrace_now() ? 8 : 0;
}

/* ---- VMODE.ASM ---- */

/* C_1336_000C: int 10h AH=0Fh */
int VID_getMode(void)
{
	return (uint8_t)uosi_bios_mode;
}

/* C_1336_001E: int 10h AH=0: mode 13h clears the video memory (unless bit 7 is set) and
 * shows A000:0000; the text mode U leaves in (3) shows nothing in the port. */
int VID_setMode(int16 mode)
{
	uosi_bios_mode = (int16)(mode & 0x7F);
	if ((mode & 0xFF) == 0x13)
		memset(u_pix(0xA000, 0), 0, 0x10000);
	plat_video_dirty();
	return 0;
}

/* ---- PALETTE.ASM ---- */

/* C_1475_0099: one DAC register from 3 words (their low bytes) */
int PAL_set(int16 index, int16 *rgb)
{
	const uint8_t *p = (const uint8_t *)rgb;

	vga_dac_write_index((uint8_t)index);
	vga_dac_write_data(p[0]);
	vga_dac_write_data(p[2]);
	vga_dac_write_data(p[4]);
	return 0;
}

/* C_1475_00B8: one DAC register into 3 words */
int PAL_get(int16 index, int16 *rgb)
{
	uint8_t *p = (uint8_t *)rgb;
	int i;

	vga_dac_read_index((uint8_t)index);
	for (i = 0; i < 3; i++) {
		p[2 * i] = vga_dac_read_data();
		p[2 * i + 1] = 0;
	}
	return 0;
}

/* C_1475_0002: the 256 colors of a palette file (at 30h, 3 bytes a color, 8 bits each,
 * to the DAC's 6) into the DAC, 16 at each vertical retrace; returns the high byte of the
 * file's word at 20h, -1 when it cannot be opened. Not called by U.
 * DOS reads the 310h bytes from 20h into a block of DOS memory without checking the
 * count (a short file leaves what the block held); the port's block is cleared. */
int PAL_load(char *name)
{
	uint8_t buf[0x310];
	int16_t h;
	uint16_t done;
	int i, bx;

	if (dosfs_open(name, 0, &h) != 0)
		return -1;
	memset(buf, 0, sizeof buf);
	dosfs_seek(h, 0, 0x20, 0);
	dosfs_read(h, buf, 0x310, &done);
	for (i = 0x2FF; i >= 0; i--)
		buf[0x10 + i] >>= 2;
	for (bx = 0; bx < 0x100; bx += 0x10) {
		VID_waitRetrace();
		vga_dac_write_index((uint8_t)bx);
		for (i = 0; i < 0x30; i++)
			vga_dac_write_data(buf[0x10 + bx * 3 + i]);
	}
	dosfs_close(h);
	return buf[1];
}
