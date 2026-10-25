/* Stand-ins for the DOS and BIOS calls of the game's C (dosint.h). */
#include "dosint.h"
#include "dosmem.h"
#include "osi.h"
#include "../platform/plat.h"
#include "../replay/replay.h"
#include "../gfx/vgadac.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- log each unknown call once ---- */

static uint32_t logged[64];
static int nlogged;

static void log_once(uint32_t key, const char *fmt, unsigned a, unsigned b)
{
	int i;
	for (i = 0; i < nlogged; i++)
		if (logged[i] == key)
			return;
	if (nlogged < 64)
		logged[nlogged++] = key;
	plat_log(fmt, a, b);
}

/* ---- drives ---- */

static int cur_drive = DOS_GAME_DRIVE;

int dos_current_drive(void) { return cur_drive; }
int dos_drive_valid(int drive) { return drive == DOS_GAME_DRIVE; }

void dos_select_drive(int drive)
{
	if (dos_drive_valid(drive))
		cur_drive = drive;
}

/* ---- clock ---- */

void dos_time_live(uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *hundredths)
{
	time_t t = time(0);
	struct tm *tm = localtime(&t);

	*hour = tm ? (uint8_t)tm->tm_hour : 0;
	*minute = tm ? (uint8_t)tm->tm_min : 0;
	*second = tm ? (uint8_t)tm->tm_sec : 0;
	*hundredths = (uint8_t)((plat_ms() % 1000u) / 10u);
}

void dos_date_live(uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *weekday)
{
	time_t t = time(0);
	struct tm *tm = localtime(&t);

	*year = tm ? (uint16_t)(tm->tm_year + 1900) : 1990;
	*month = tm ? (uint8_t)(tm->tm_mon + 1) : 1;
	*day = tm ? (uint8_t)tm->tm_mday : 1;
	*weekday = tm ? (uint8_t)tm->tm_wday : 1;
}

/* ---- int 21h ---- */

static void fail(union REGS *r, uint16_t err)
{
	r->x.ax = err;
	r->x.cflag = 1;
}

static void int21(union REGS *r, struct SREGS *s)
{
	uint8_t ah = r->h.ah;

	(void)s;
	r->x.cflag = 0;
	switch (ah) {
	case 0x0E:   /* select disk; AL = number of drives (LASTDRIVE=E) */
		dos_select_drive(r->h.dl);
		r->h.al = 5;
		break;
	case 0x19:   /* current disk */
		r->h.al = (uint8_t)cur_drive;
		break;
	case 0x2A: { /* get date */
		uint16_t y;
		uint8_t m, d, w;
		uint32_t v;
		dos_date_live(&y, &m, &d, &w);
		v = rp_value(RP_DATE, ((uint32_t)y << 16) | ((uint32_t)m << 8) | d);
		r->x.cx = (uint16_t)(v >> 16);
		r->h.dh = (uint8_t)(v >> 8);
		r->h.dl = (uint8_t)v;
		r->h.al = (uint8_t)rp_value(RP_DATE, w);
		break;
	}
	case 0x2C: { /* get time */
		uint8_t h, m, sec, hs;
		uint32_t v;
		dos_time_live(&h, &m, &sec, &hs);
		v = rp_value(RP_DATE, ((uint32_t)h << 24) | ((uint32_t)m << 16) | ((uint32_t)sec << 8) | hs);
		r->h.ch = (uint8_t)(v >> 24);
		r->h.cl = (uint8_t)(v >> 16);
		r->h.dh = (uint8_t)(v >> 8);
		r->h.dl = (uint8_t)v;
		break;
	}
	case 0x30:   /* DOS version */
		r->h.al = DOS_VERSION_MAJOR;
		r->h.ah = DOS_VERSION_MINOR;
		r->h.bh = 0xFF;   /* OEM: Microsoft */
		r->h.bl = 0;
		r->x.cx = 0;
		break;
	case 0x44: { /* IOCTL; BL = drive, 0 = default */
		int drive = r->h.bl ? r->h.bl - 1 : cur_drive;
		if (r->h.al == 0x08) {        /* removable? AX 0 yes, 1 no */
			if (!dos_drive_valid(drive))
				fail(r, 0x0F);
			else
				r->x.ax = 1;
		} else if (r->h.al == 0x0E) { /* logical drive map: AL 0 = one name only */
			if (!dos_drive_valid(drive))
				fail(r, 0x0F);
			else
				r->h.al = 0;
		} else {
			log_once(0x214400u | r->h.al, "dosint: int 21h AX=44%02Xh not emulated", r->h.al, 0);
			fail(r, 1);
		}
		break;
	}
	case 0x4C:
		dos_exit(r->h.al);
	default:
		log_once(0x210000u | ((uint32_t)ah << 8), "dosint: int 21h AH=%02Xh not emulated", ah, 0);
		fail(r, 1);
		break;
	}
}

/* ---- int 10h: the VGA DAC ---- */

static void int10(union REGS *r, struct SREGS *s)
{
	unsigned i;

	switch (r->x.ax) {
	case 0x1010:   /* set one DAC register BX to DH, CH, CL */
		vga_dac_write_index((uint8_t)r->x.bx);
		vga_dac_write_data(r->h.dh);
		vga_dac_write_data(r->h.ch);
		vga_dac_write_data(r->h.cl);
		return;
	case 0x1012: { /* set CX DAC registers from BX, table at ES:DX */
		const uint8_t *p;
		if (!s) {
			log_once(0x101200u, "dosint: int 10h AX=1012h without ES (int86)", 0, 0);
			return;
		}
		p = (const uint8_t *)dos_mk_fp(s->es, r->x.dx);
		vga_dac_write_index((uint8_t)r->x.bx);
		for (i = 0; i < 3u * r->x.cx; i++)
			vga_dac_write_data(p[i]);
		return;
	}
	case 0x1015:   /* read one DAC register */
		vga_dac_read_index((uint8_t)r->x.bx);
		r->h.dh = vga_dac_read_data();
		r->h.ch = vga_dac_read_data();
		r->h.cl = vga_dac_read_data();
		return;
	case 0x1017: { /* read CX DAC registers into ES:DX */
		uint8_t *p;
		if (!s)
			return;
		p = (uint8_t *)dos_mk_fp(s->es, r->x.dx);
		vga_dac_read_index((uint8_t)r->x.bx);
		for (i = 0; i < 3u * r->x.cx; i++)
			p[i] = vga_dac_read_data();
		return;
	}
	default:
		log_once(0x100000u | r->x.ax, "dosint: int 10h AX=%04Xh not emulated", r->x.ax, 0);
		return;
	}
}

static int do_int(int intno, union REGS *in, union REGS *out, struct SREGS *s)
{
	union REGS r = *in;

	r.x.cflag = 0;
	switch ((uint8_t)intno) {
	case 0x10:
		int10(&r, s);
		break;
	case 0x11:   /* equipment list: no diskette, 80x25 colour */
		r.x.ax = 0x0020;
		break;
	case 0x12:   /* memory size in KB */
		r.x.ax = 640;
		break;
	case 0x21:
		int21(&r, s);
		break;
	default:
		log_once(0x10000000u | (uint8_t)intno, "dosint: int %02Xh not emulated", (uint8_t)intno, 0);
		break;
	}
	r.x.flags = (uint16_t)(0x3202u | (r.x.cflag ? 1u : 0u));   /* IF set, CF from cflag */
	*out = r;
	return (int16_t)r.x.ax;
}

int int86(int intno, union REGS *inregs, union REGS *outregs)
{
	return do_int(intno, inregs, outregs, 0);
}

int int86x(int intno, union REGS *inregs, union REGS *outregs, struct SREGS *segregs)
{
	return do_int(intno, inregs, outregs, segregs);
}

int intdos(union REGS *inregs, union REGS *outregs)
{
	return do_int(0x21, inregs, outregs, 0);
}

int intdosx(union REGS *inregs, union REGS *outregs, struct SREGS *segregs)
{
	return do_int(0x21, inregs, outregs, segregs);
}

/* The game's segment registers: there is one data segment, which the port does not
 * have; DS = SS = ES = a foreign segment, CS = 0. Only SetVGAPalette calls this, and it
 * replaces ES before use. */
void segread(struct SREGS *segp)
{
	segp->cs = 0;
	segp->ds = segp->ss = segp->es = DOS_SEG_FOREIGN;
}

/* ---- I/O ports ---- */

void outportb(int portid, unsigned char value)
{
	switch ((uint16_t)portid) {
	case 0x3C7: vga_dac_read_index(value); break;
	case 0x3C8: vga_dac_write_index(value); break;
	case 0x3C9: vga_dac_write_data(value); break;
	default:
		log_once(0x20000000u | (uint16_t)portid, "dosint: outportb(%Xh, %Xh) ignored",
		         (uint16_t)portid, value);
		break;
	}
}

unsigned char inportb(int portid)
{
	static uint8_t retrace;

	switch ((uint16_t)portid) {
	case 0x3C9:
		return vga_dac_read_data();
	case 0x3DA:   /* input status: toggles retrace so that wait loops end */
		retrace ^= 0x09;
		return retrace;
	default:
		log_once(0x30000000u | (uint16_t)portid, "dosint: inportb(%Xh) gives 0FFh",
		         (uint16_t)portid, 0);
		return 0xFF;
	}
}

/* ---- exec, exit ---- */

static dos_exec_handler exec_handler;
static int exec_calls;

void dos_set_exec_handler(dos_exec_handler handler)
{
	exec_handler = handler;
}

int dos_execl_list(int n, const char *const *args)
{
	int argc = 0;

	while (argc + 1 < n && args[argc + 1])
		argc++;
	if (exec_calls++ == 0) {
		char line[512];
		int i;
		size_t l;

		snprintf(line, sizeof line, "dosint: execl(\"%s\"", args[0] ? args[0] : "");
		for (i = 1; i <= argc; i++) {
			l = strlen(line);
			snprintf(line + l, sizeof line - l, ", \"%s\"", args[i]);
		}
		l = strlen(line);
		snprintf(line + l, sizeof line - l, "): cannot run DOS programs, failing as DOS would");
		plat_log("%s", line);
		if (exec_handler)
			exec_handler(args[0], argc, args + 1);
	} else {
		/* the game retries forever: wait for the user (or the replay) to end it */
		plat_pump();
		if (plat_quit_requested() || (rp_mode == RP_REPLAY && rp_finished()))
			dos_exit(0);
		if (!osi_replaying())
			plat_sleep_ms(20);
	}
	errno = ENOENT;
	return -1;
}

#define EXECL_MAX 16

int execl(const char *path, const char *arg0, ...)
{
	const char *args[EXECL_MAX + 2];
	int n = 0;
	va_list ap;

	args[n++] = path;
	args[n++] = arg0;
	if (arg0) {
		va_start(ap, arg0);
		while (n < EXECL_MAX + 1) {
			const char *a = va_arg(ap, const char *);
			if (((uintptr_t)a & 0xFFFFFFFFu) == 0)
				break;
			args[n++] = a;
		}
		va_end(ap);
	}
	args[n++] = 0;
	return dos_execl_list(n, args);
}

void (*dos_exit_hook)(int code);

void dos_exit(int code)
{
	plat_speaker(0);
	if (dos_exit_hook)
		dos_exit_hook(code);
	plat_log("dosint: exit(%d)", code);
	plat_shutdown();
	exit(code);
}

void dos_divide_error(const char *where)
{
	plat_speaker(0);
	plat_log("dosint: divide error in %s (the DOS game would stop here)", where);
	plat_fatal("Divide error");
	exit(3);
}
