/*
 * Ultima VI - The False Prophet: U.EXE
 * segment 0BC1: disks and drives, palette
 */
#include "u.h"
#include <alloc.h>
#include <conio.h>

extern int videoMode;/*video mode: 0x0d EGA, 0x13 MCGA*/
extern char mouseL, mouseR;
extern int menuDrawn;
extern int escaped;
extern char dosMajor, dosMinor;/*DOS version: major, minor*/
extern struct tSurface screen, backBuffer;

/*names of the disks, as the disk prompt says them*/
/*4038*/char *diskNames[] = {"", "Program", "Intro.", "Endgame", "Game", "Map", "Conversation", "Portrait", "Player"};
/*404A*/int curDisk = 1;/*the disk in the current drive*/
/*404C*/int wantDisk = 0;/*the disk asked for*/
/*404E*/char diskDrive[9] = {0};/*drive of each game disk (1 = A:)*/
/*4057*/char driveKind[26] = {0};/*kind of each drive: 0 none, 1 fixed, 2 removable*/
/*4071*/char markerFile[] = "u6.1";/*name of a disk's marker file; [3] is the disk number*/
/*4076*/char mouseWasDown = 0;/*a mouse button was down: wait for it to be released*/
/*4077*/char mouseHeld = 0;

/*C_0BC1_0000*/int IsDiskIn(int n) {
	int fd;

	markerFile[3] = n + '0';
	fd = OSI_open(markerFile);
	if(fd != -1)
		OSI_close(fd);
	return fd != -1;
}

/*C_0BC1_0031*/SelectDiskDrive(int n) {
	int drive;
	int i;

	drive = diskDrive[n];
	bdos(0xe, drive - 1, 0);
	for(i = 1; i <= 8; i++)
		if(IsDiskIn(i))
			diskDrive[i] = drive;
}

/*C_0BC1_0079*/AskForDisk(int n) {
	int i;
	int found;
	int drive;

	drive = (bdos(0x19, 0, 0) & 0xff) + 'A';
	while(curDisk != n && diskDrive[n] == 0) {
		wantDisk = n;
		if(videoMode == 0xd)
			backBuffer = screen;
		DRV(0xc)(&backBuffer, 0);
		menuDrawn = 0;
		ShowPassage(999);
		DisplayPassage();
		SetPalette();
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
			backBuffer = screen;
		}
		i = 0;
		while(!i) {
			MOUSE_SERVE();
			if(mouseL || mouseR) {
				if(!mouseWasDown) {
					mouseWasDown = mouseHeld = 1;
					i = 0x20;
				} else {
					mouseHeld = 0;
				}
			} else {
				mouseWasDown = mouseHeld = 0;
			}
			if(!i && kbhit())
				i = 0x20;
		}
		DRV(0xc)(&backBuffer, 0);
		if(videoMode != 0xd) {
			DRV(0xf)(&backBuffer, &screen);
		} else {
			DRV(0x15)(&screen);
			backBuffer = screen;
			DRV(0xc)(&backBuffer, 0);
		}
		if(kbhit())
			if(getch() == 0x1b)
				escaped = 1;
		found = 0;
		for(i = 0; i < 0x1a; i++) {
			if(driveKind[i] == 2) {
				bdos(0xe, i, 0);
				if(IsDiskIn(n)) {
					found = i + 1;
					diskDrive[n] = found;
					for(i = 1; i <= 8; i++)
						if(i != n && diskDrive[i] == found)
							diskDrive[i] = 0;
					curDisk = n;
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
	dosMajor = regs.h.al;
	dosMinor = regs.h.ah;
	if(dosMajor >= 3) {
		for(i = 0; i < 0x1a; i++) {
			regs.h.ah = 0x44;
			regs.h.al = 8;
			regs.h.bl = i + 1;
			intdos(&regs, &regs);
			if(regs.x.cflag == 0) {
				if(regs.x.ax)
					driveKind[i] = 1;
				else
					driveKind[i] = 2;
				if(dosMajor >= 4 || dosMinor >= 2) {
					regs.h.ah = 0x44;
					regs.h.al = 0xe;
					regs.h.bl = i + 1;
					intdos(&regs, &regs);
					if(regs.h.al && regs.h.al != i + 1)
						driveKind[i] = 0;
				}
			}
		}
	}
	if(dosMajor < 3 || (dosMajor == 3 && dosMinor < 2)) {
		int86(0x11, &regs, &regs);
		if(regs.x.ax & 1) {
			driveKind[0] = 2;
			if(regs.x.ax & 0xc0)
				driveKind[1] = 2;
		} else {
			driveKind[0] = driveKind[1] = 0;
		}
		i = bdos(0x19, 0, 0) & 0xff;
		if(i > 1)
			driveKind[i] = 1;
	}
	curDisk = 1;
	diskDrive[1] = (bdos(0x19, 0, 0) & 0xff) + 1;
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

	if(videoMode == 0x13) {
		buf = farmalloc(0x300L);
		fd = OSI_open("palettes.int");
		if(fd >= 0) {
			ofs = n;
			ofs <<= 6;
			ofs += ofs << 3;
			OSI_read(fd, ofs, 0x240L, buf);
			OSI_close(fd);
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
					VID_waitRetrace();
				PAL_set(i, rgb);
			}
		}
		farfree(buf);
	}
}
