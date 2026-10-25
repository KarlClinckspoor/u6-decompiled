/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	Drive/floppy management module.

	The game came on 8 floppy disks (see DiskNames; DISK_PROGRAM..DISK_PLAYER), each one
	identified by a file "U6.<n>" in its root directory.  DISK_init()
	finds the drives (DOS IOCTL / BIOS equipment list) and assumes the
	current one holds the Game disk.  DiskDrive[] remembers on which drive
	each disk was found (1 = A:, 0 = unknown), so on a hard disk install
	every disk resolves to the same drive and no prompt ever appears.
	DISK_confirm(n) must be called before accessing the files of disk n:
	it selects the right drive, or asks the player to insert the disk and
	searches all the removable drives for it.
	DISK_full() is called by the file error handler on a short write.
*/
#include "u6.h"

/*disk names, indexed by DISK_x*/
/*2E88*/char *DiskNames[] = {
	/*2EC8*/"",
	/*2EC9*/"Program",
	/*2ED1*/"Introduction",
	/*2EDE*/"Endgame",
	/*2EE6*/"Game",
	/*2EEB*/"Map",
	/*2EEF*/"Populace A",
	/*2EFA*/"Populace B",
	/*2F05*/"Player"
};
/*2E9A*/int16 CurrentDisk = 0;/*current disk (DISK_x)*/
/*2E9C*/int16 CurrentDrive = 1;/*current drive (1 = A:)*/
/*2E9E*/int16 DiskFullChoice = ' ';/*"disk full" answer: 'S'ave/'R'estore, 'G' once saved, ' ' none*/
/*2EA0*/unsigned char DiskDrive[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};/*drive of each disk (1 = A:, 0 = unknown)*/
/*2EA9*/unsigned char DriveType[26] = {0};/*drive types A:..Z: 0 none, 1 fixed, 2 removable*/
/*2EC3*/char DiskIdFile[] = "u6.1";/*disk identification file, [3] = disk number*/


/*default "drive selection failed" callback (DriveErrorCallback): do nothing*/
/*DISK_0005*/static DISK_NullCallback() {
	return 0;
}

/*is disk "diskNum" in the current drive? (does "U6.<diskNum>" exist)
  The file errors are silenced while probing*/
/*DISK_0008*/static DISK_IsInDrive(int16 diskNum) {
	int16 fileHandle;

	OSI_ErrorHandler.f = OSI_callback;
	DiskIdFile[3] = diskNum + '0';
	fileHandle = OSI_open(DiskIdFile);
	if(fileHandle != -1)
		OSI_close(fileHandle);
	OSI_ErrorHandler.f = FileErrorHandler;
	return (fileHandle != -1);
}

/*select the drive of disk "diskNum" and register this drive for every
  other disk whose identification file is also there*/
/*DISK_0053*/static DISK_ScanDrive(int16 diskNum) {
	int16 disk;
	int16 drive;

	CurrentDrive =
	drive = DiskDrive[diskNum];
	/*selectDrive*/OSI_selectDrive(drive + 'A' - 1);
	for(disk = DISK_PROGRAM; disk <= DISK_PLAYER; disk++)
		if(DISK_IsInDrive(disk))
			DiskDrive[disk] = drive;
}

/*make sure disk "diskNum" is available and its drive selected; prompts
  the player to insert it when its drive is not known*/
/*DISK_0097*/DISK_confirm(int16 diskNum/*bp06*/) {
	int16 keyOrIndex;
	int16 foundDrive;
	struct tClipInfo *savedClip;

	if(CurrentDisk == diskNum)
		return;
	while(CurrentDisk != diskNum && DiskDrive[diskNum] == 0) {
		savedClip = CurClip;
		CON_setClip(&(TextWindows[3]));
		CON_printf(InsertDiskMsg, DiskNames[diskNum]);
		CON_putch0(-1);
		AnimateAtPrompt = 0;/*no animations while waiting*/
		/*wait for a key or a mouse click (= ' ')*/
		keyOrIndex = 0;
		while(!keyOrIndex) {
			if(MouseOn) {
				MOUSE_SERVE();
				if(Mouse.buttonL || Mouse.buttonR) {
					if(!MousePress) {
						MousePress =
						MouseTrigger = 1;
						keyOrIndex = ' ';
					} else {
						MouseTrigger = 0;
					}
				} else {
					MousePress =
					MouseTrigger = 0;
				}
			}
			if(!keyOrIndex)
				keyOrIndex = CON_prompt();
		}
		CON_putch0('\n');
		CON_setClip(savedClip);
		AnimateAtPrompt = 1;
		/*<CTRL+C>: accept the current drive as is.
		  bug?: CurrentDrive is a drive number here, not a letter, so the
		  stored drive is wrong and the drive selection fails*/
		if(keyOrIndex == 3) {
			CurrentDisk = diskNum;
			DiskDrive[diskNum] = CurrentDrive - ('A' - 1);
			/*selectDrive*/OSI_selectDrive(CurrentDrive);
			return;
		}
		/*search the disk in all the removable drives*/
		foundDrive = 0;
		for(keyOrIndex = 0; keyOrIndex < 26; keyOrIndex++) {
			if(DriveType[keyOrIndex] != 2)
				continue;
			/*selectDrive*/OSI_selectDrive(keyOrIndex + 'A');
			if(DISK_IsInDrive(diskNum)) {
				foundDrive = keyOrIndex + 1;
				DiskDrive[diskNum] = foundDrive;
				/*the other disks are not in this drive anymore*/
				for(keyOrIndex = DISK_PROGRAM; keyOrIndex <= DISK_PLAYER; keyOrIndex++) {
					if(keyOrIndex != diskNum && DiskDrive[keyOrIndex] == foundDrive)
						DiskDrive[keyOrIndex] = 0;
				}
				DISK_ScanDrive(diskNum);
				break;
			}
		}
	}
	if(DiskDrive[diskNum] != CurrentDrive) {
		CurrentDrive = DiskDrive[diskNum];
		/*selectDrive*/OSI_selectDrive(CurrentDrive + 'A' - 1);
	}
	CurrentDisk = diskNum;
}

/*uncalled?: forget the drive of the current disk and ask for it again*/
/*DISK_0202*/DISK_Reconfirm() {
	DiskDrive[CurrentDisk] = 0;
	DISK_confirm(CurrentDisk);
}

/*"disk full" while writing: ask whether to save the game (S) or restore
  the last saved game (R); the answer is stored in DiskFullChoice*/
/*DISK_0211*/DISK_full() {
	int16 answer;

	MapAnimEnabled = 0;
	CON_printf("Disk full.  Do you want to Save the game and continue, ");
	CON_printf("or Restore your last saved game? ");
	do {
		answer = CON_getch();
	} while(answer != 'S' && answer != 'R');
	DiskFullChoice = answer;
	if(answer == 'S')
		CON_printf("S\n\nSaving...\n");
	else
		CON_printf("R\n\nRestoring...\n");
}

/*find the drives and their types, assume the current drive holds the
  Game disk (DISK_GAME)*/
/*DISK_0266*/DISK_init() {
	int16 drive;
	union REGS regs;

	DriveErrorCallback = DISK_NullCallback;
	/*get dos version*/
	regs.h.ah = 0x30;
	intdos(&regs, &regs);
	DosVerMajor = regs.h.al;
	DosVerMinor = regs.h.ah;

	if(DosVerMajor >= 3) {
		for(drive = 0; drive < 26; drive++) {
			/*IOCTL 4408h: is drive removable? (ax: 0 yes, 1 no)*/
			regs.h.ah = 0x44;
			regs.h.al = 8;
			regs.h.bl = drive + 1;
			intdos(&regs, &regs);
			if(regs.x.cflag != 0)
				continue;
			if(regs.x.ax)
				DriveType[drive] = 1;
			else
				DriveType[drive] = 2;
			/*dos 3.20 and above: IOCTL 440Eh get logical drive map;
			  a drive which is an alias of another one (e.g. B: on
			  single floppy systems) is ignored*/
			if(DosVerMajor >= 4 || DosVerMinor >= 20) {
				regs.h.ah = 0x44;
				regs.h.al = 0xe;
				regs.h.bl = drive + 1;
				intdos(&regs, &regs);
				if(regs.h.al && regs.h.al != drive + 1)
					DriveType[drive] = 0;
			}
		}
	}
	/*below dos 3.20: BIOS equipment list (int 11h): bit 0 floppy
	  installed, bits 6-7 number of floppies - 1; the current drive is
	  considered fixed if it is not A: or B:*/
	if(DosVerMajor < 3 || (DosVerMajor == 3 && DosVerMinor < 20)) {
		int86(0x11, &regs, &regs);
		if(regs.x.ax & 1) {
			DriveType[0] = 2;
			if(regs.x.ax & 0xc0)
				DriveType[1] = 2;
		} else {
			DriveType[0] =
			DriveType[1] = 0;
		}
		drive = /*getDefaultDrive*/OSI_getDrive();
		if(drive > 'B')
			DriveType[drive - 'A'] = 1;
	}

	CurrentDisk = DISK_GAME;
	DiskDrive[DISK_GAME] = /*getDefaultDrive*/OSI_getDrive() + 1 - 'A';
	DISK_ScanDrive(DISK_GAME);
}
