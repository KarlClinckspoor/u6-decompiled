/* Test shim for port/sys/adlib.c (port/tests/adlib_diff.py): the OPL chip records the
 * register writes, the platform's audio runs the music function on demand, and the songs
 * live in the DOS arena. */
#include <stdint.h>
#include <string.h>

#include "../sys/dosmem.h"
#include "../platform/plat.h"
#include "../third_party/nuked-opl3/opl3.h"

void MUSDRV_0(int port);
void MUSDRV_3(void);
void MUSDRV_6(void *song);
void MUSDRV_9(void);

static plat_music_fn music;
static uint16_t log_buf[1 << 16];
static int log_n;

void plat_set_music(plat_music_fn fn) { music = fn; }
void plat_audio_lock(void) { }
void plat_audio_unlock(void) { }
void plat_log(const char *fmt, ...) { (void)fmt; }
void plat_fatal(const char *m) { (void)m; }

void OPL3_Reset(opl3_chip *chip, uint32_t samplerate) { (void)chip; (void)samplerate; }
/* (writes the reset replays are not logged: see t_prime) */
void OPL3_WriteReg(opl3_chip *chip, uint16_t reg, uint8_t v)
{
	(void)chip;
	if (log_n < (int)(sizeof log_buf / sizeof log_buf[0]))
		log_buf[log_n++] = (uint16_t)(reg << 8 | v);
}
void OPL3_WriteRegBuffered(opl3_chip *chip, uint16_t reg, uint8_t v) { OPL3_WriteReg(chip, reg, v); }
void OPL3_GenerateResampled(opl3_chip *chip, int16_t *buf) { (void)chip; buf[0] = buf[1] = 0; }

/* ---- for the test ---- */

void t_init(void) { dosmem_init(); }
uint8_t *t_arena(void) { return dos_base; }
void t_drv0(int port) { MUSDRV_0(port); }
void t_drv3(void) { MUSDRV_3(); }
void t_drv6(uint16_t seg, uint16_t off) { MUSDRV_6(dos_mk_fp(seg, off)); }
void t_drv9(void) { MUSDRV_9(); }
/* one tick of the int 8 handler: one frame at 60 Hz */
void t_tick(void)
{
	int16_t s = 0;
	if (music)
		music(&s, 1, 60);
}
int t_log(uint16_t *out, int max)
{
	int n = log_n < max ? log_n : max;
	memcpy(out, log_buf, (size_t)n * 2);
	log_n = 0;
	return n;
}
/* the chip is set up at the first call of the music function (the rate) */
void t_prime(void)
{
	int16_t s = 0;
	if (music)
		music(&s, 0, 60);
}
uint8_t *adlib_state(void);
uint8_t *t_state(void) { return adlib_state(); }
