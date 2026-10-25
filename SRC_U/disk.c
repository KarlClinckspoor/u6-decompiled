/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0BC1: disks and drives, palette
 */
#include "u.h"
#include <alloc.h>
#include <conio.h>

extern int D_5044;/*video mode: 0x0d EGA, 0x13 MCGA*/
extern char D_5046, D_5047;
extern int D_575A;
extern int D_58F0;
extern char D_6094, D_6095;/*DOS version: major, minor*/
extern struct tSurface D_5710, D_5732;

/*names of the disks, as the disk prompt says them*/
/*4038*/char *D_4038[] = {"", "Program", "Intro.", "Endgame", "Game", "Map", "Conversation", "Portrait", "Player"};
/*404A*/int D_404A = 1;/*the disk in the current drive*/
/*404C*/int D_404C = 0;/*the disk asked for*/
/*404E*/char D_404E[9] = {0};/*drive of each game disk (1 = A:)*/
/*4057*/char D_4057[26] = {0};/*kind of each drive: 0 none, 1 fixed, 2 removable*/
/*4071*/char D_4071[] = "u6.1";/*name of a disk's marker file; [3] is the disk number*/
/*4076*/char D_4076 = 0;/*a mouse button was down: wait for it to be released*/
/*4077*/char D_4077 = 0;

/*C_0BC1_0000*/int IsDiskIn(int n) {
	int fd;

	D_4071[3] = n + '0';
	fd = C_0000_09F3(D_4071);
	if(fd != -1)
		C_0000_0A32(fd);
	return fd != -1;
}

/*C_0BC1_0031*/SelectDiskDrive(int n) {
	int drive;
	int i;

	drive = D_404E[n];
	bdos(0xe, drive - 1, 0);
	for(i = 1; i <= 8; i++)
		if(IsDiskIn(i))
			D_404E[i] = drive;
}

/*C_0BC1_0079*/AskForDisk(int n) {
	int i;
	int found;
	int drive;

	drive = (bdos(0x19, 0, 0) & 0xff) + 'A';
	while(D_404A != n && D_404E[n] == 0) {
		D_404C = n;
		if(D_5044 == 0xd)
			D_5732 = D_5710;
		DRV(0xc)(&D_5732, 0);
		D_575A = 0;
		C_09A0_000B(999);
		C_09A0_21C5();
		C_00E1_04F9();
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
			D_5732 = D_5710;
		}
		i = 0;
		while(!i) {
			C_132C_0008();
			if(D_5046 || D_5047) {
				if(!D_4076) {
					D_4076 = D_4077 = 1;
					i = 0x20;
				} else {
					D_4077 = 0;
				}
			} else {
				D_4076 = D_4077 = 0;
			}
			if(!i && kbhit())
				i = 0x20;
		}
		DRV(0xc)(&D_5732, 0);
		if(D_5044 != 0xd) {
			DRV(0xf)(&D_5732, &D_5710);
		} else {
			DRV(0x15)(&D_5710);
			D_5732 = D_5710;
			DRV(0xc)(&D_5732, 0);
		}
		if(kbhit())
			if(getch() == 0x1b)
				D_58F0 = 1;
		found = 0;
		for(i = 0; i < 0x1a; i++) {
			if(D_4057[i] == 2) {
				bdos(0xe, i, 0);
				if(IsDiskIn(n)) {
					found = i + 1;
					D_404E[n] = found;
					for(i = 1; i <= 8; i++)
						if(i != n && D_404E[i] == found)
							D_404E[i] = 0;
					D_404A = n;
					break;
				}
			}
		}
	}
	SelectDiskDrive(n);
}

/*C_0BC1_027D*/FindDrives() {
	union REGS regs;
	int i;

	regs.h.ah = 0x30;
	intdos(&regs, &regs);
	D_6094 = regs.h.al;
	D_6095 = regs.h.ah;
	if(D_6094 >= 3) {
		for(i = 0; i < 0x1a; i++) {
			regs.h.ah = 0x44;
			regs.h.al = 8;
			regs.h.bl = i + 1;
			intdos(&regs, &regs);
			if(regs.x.cflag == 0) {
				if(regs.x.ax)
					D_4057[i] = 1;
				else
					D_4057[i] = 2;
				if(D_6094 >= 4 || D_6095 >= 2) {
					regs.h.ah = 0x44;
					regs.h.al = 0xe;
					regs.h.bl = i + 1;
					intdos(&regs, &regs);
					if(regs.h.al && regs.h.al != i + 1)
						D_4057[i] = 0;
				}
			}
		}
	}
	if(D_6094 < 3 || (D_6094 == 3 && D_6095 < 2)) {
		int86(0x11, &regs, &regs);
		if(regs.x.ax & 1) {
			D_4057[0] = 2;
			if(regs.x.ax & 0xc0)
				D_4057[1] = 2;
		} else {
			D_4057[0] = D_4057[1] = 0;
		}
		i = bdos(0x19, 0, 0) & 0xff;
		if(i > 1)
			D_4057[i] = 1;
	}
	D_404A = 1;
	D_404E[1] = (bdos(0x19, 0, 0) & 0xff) + 1;
	SelectDiskDrive(1);
}

/*C_0BC1_03D2*/LoadPalette(int n) {
	int k;
	long ofs;
	int fd;
	unsigned char far *buf;
	unsigned char far *p;
	int rgb[3];
	int i, j;

	if(D_5044 == 0x13) {
		buf = farmalloc(0x300L);
		fd = C_0000_09F3("palettes.int");
		if(fd >= 0) {
			ofs = n;
			ofs <<= 6;
			ofs += ofs << 3;
			C_0000_0AE4(fd, ofs, 0x240L, buf);
			C_0000_0A32(fd);
			for(i = 0xbf; i >= 0; i--) {
				j = i * 3;
				k = i * 4;
				buf[k + 3] = buf[j + 2] >> 2;
				buf[k + 2] = ((buf[j + 2] << 4) & 0x30) + (buf[j + 1] >> 4);
				buf[k + 1] = ((buf[j + 1] << 2) & 0x3c) + (buf[j] >> 6);
				buf[k] = buf[j] & 0x3f;
			}
			p = buf;
			for(i = 0; i < 0x100; i++) {
				rgb[0] = *p++;
				rgb[1] = *p++;
				rgb[2] = *p++;
				if(!(i & 0xf))
					C_1368_0015();
				C_1475_0099(i, rgb);
			}
		}
		farfree(buf);
	}
}
