/*
 * Ultima VI - The False Prophet
 * Copyright (c) 1990 Origin Systems, Inc. and Richard Gariott
 * reverse-coded by Ergonomy Joe in 2013
 */
/*
	Main/startup module.

	main() parses the configuration (CONFIG.U6 + command line), grabs all
	the far heap with farmalloc() and then hands out pieces of it with the
	simple bump allocator __MemAlloc() (Mem_BP/Mem_Remain).  It sets up the
	mouse and the screen surfaces (scanline offset tables for VGA/EGA/
	Tandy/CGA/Hercules), loads the graphic and music drivers, the font,
	palette, tile tables and animation data, loads the saved game and
	finally runs the game loop (GameLoop).
	Also here: the generic file loader LoadFile() (plain or LZW "Z" files),
	the loader callback given to the graphic driver (LoadFileGr), the OSI
	file error handler (FileErrorHandler), the VGA palette cycling for fire/
	water colors (PaletteAnimation) and the launch of END.EXE/ULTIMA6.EXE.
*/
#include "u6.h"

/*====---- ----====*/
/*0194*/int16 IsZFile = 0;/*1: LoadFile() decompresses LZW (".z") files*/
/*list of file names the OSI loader (LOW.ASM C_049C) would treat as
  compressed; empty list (only the "" terminator)*/
/*0196*/char *CompressedFileList[] = {
	/*01EB*/""
};
/*full screen bitmaps*/
/*0198*/char *BitmapFiles[] = {
	/*01EC*/"paper.bmp",/*main screen*/
	/*01F6*/"newmagic.bmp",/*small scroll*/
	/*0203*/"u6face0.bmp",
	/*020F*/"stats.bmp",
	/*0219*/"worldmap.bmp"
};
/*01A2*/char DirIncrX[] = { 0, 1, 1, 1, 0,-1,-1,-1};
/*01AA*/char DirIncrY[] = {-1,-1, 0, 1, 1, 1, 0,-1};
/*01B2*/char *DirNames[] = {
	/*0226*/"North",
	/*022C*/"Northeast",
	/*0236*/"East",
	/*023B*/"Southeast",
	/*0245*/"South",
	/*024B*/"Southwest",
	/*0255*/"West",
	/*025A*/"Northwest"
};
int16 Unused01C2 = 0;/*unused*/
int16 Unused01C4 = 0;/*unused*/
/*01C6*/int16 IgnoreShortRead = 1;/*1: ignore "short read" errors (OSI_read hit end of file)*/
/*01C8*/int16 GrAdapter = 0;/*graphic adapter: 0 VGA/MCGA, 1 EGA, 2 Tandy, 3 CGA, 4 Hercules*/
/*01CA*/int16 MusicAdapter = 0;/*music adapter: 0 none, 1 AdLib, 2 CMS, 3 Tandy, 4 Roland, 5 Covox, 6 Innovation*/
/*01CC*/int16 MusicPort = 0x388;/*music card i/o port*/
/*01CE*/int16 GrDriverOption = 0;/*'$' config flag, added to the init parameter of the graphic driver (see InitGraphicsDriver); meaning unknown*/
/*01D0*/unsigned char MouseOn = 1;
/*01D1*/char ConsoleReady = 0;/*1 once the console can display messages (error handler)*/
/*01D2*/unsigned char PaletteCycle = 0;/*palette cycling counter*/
/*01D3*/char GrAdapterLetters[] = "vetch";/*config letters for GrAdapter: Vga/Ega/Tandy/Cga/Hercules*/
/*01D9*/char MusicAdapterLetters[] = "naxyrzi";/*config letters for MusicAdapter: None/Adlib/cms(X)/tandY/Roland/covoX(Z)/Innovation*/
/*====---- ----====*/

/*far pointer <-> 20-bit linear address*/
#define PTR2NORM(p) (((uint32)FP_SEG(p) << 4) + FP_OFF(p))
#define NORM2PTR(n) (MK_FP((n) >> 4, (n) & 0xf))

/*allocate "size" zeroed bytes from the far heap block (Mem_BP/Mem_Remain);
  exits to DOS if there is not enough memory.
  The allocator never frees: memory is handed out once at startup*/
/*C_0903_0001*/static __MemAlloc(char huge **pptr, uint32 size) {
	uint32 offset, linearAddr;

	if(size > Mem_Remain) {
		/*not enough memory*/
		OSI_delay_clean();
		GR_SHUTDOWN();
		exit(1);
		return;
	}

	*pptr = Mem_BP;
	/*update remaining memory and base pointer*/
	linearAddr = PTR2NORM(Mem_BP);
	linearAddr += size;
	Mem_BP = NORM2PTR(linearAddr);
	Mem_Remain -= size;
	/*zero memory*/
	for(offset = 0; offset < size; offset ++)
		*(*pptr + offset) = 0; 
}

/*load a whole file.
  pBuffer == 0: the file is loaded into newly allocated (paragraph aligned)
  far heap memory; returns 0 if the file is missing or too big.
  pBuffer != 0: the file is loaded into pBuffer; returns pBuffer.
  If IsZFile is set, the file is LZW compressed: its first 4 bytes hold the
  uncompressed size and decompress() needs a work area (hash table, input
  buffer) given as a segment located after the destination data*/
/*C_0903_00DE*/void far *LoadFile(char *fname, void far *pBuffer) {
	int16 fileHandle;
	uint16 aSegment;
	void far *pRetBuff;
	int32 linearAddr;
	int32 fsize;

	if(pBuffer == 0) {
		/*para align*/
		linearAddr = PTR2NORM(Mem_BP);
		if(linearAddr & 0xf) {
			Mem_Remain -= 0x10 - (linearAddr & 0xf);
			linearAddr = (linearAddr & ~0xf) + 0x10;
			Mem_BP = MK_FP(linearAddr >> 4, 0);
		}

		fileHandle = OSI_open(fname);
		pRetBuff = 0;
		if(fileHandle >= 0) {
			if(IsZFile) {
				/*uncompressed size*/
				OSI_read(fileHandle, 0, 4, &fsize);
				/*data + work area (hash table + input buffer)*/
				if(fsize + 0x3410 <= Mem_Remain) {
					pRetBuff = Mem_BP;
					decompress(fileHandle, 0L, Mem_BP, FP_SEG(Mem_BP) + ((fsize + 0xf) >> 4));
				}
			} else {
				fsize = OSI_fileSize(fileHandle);
				if(fsize <= Mem_Remain) {
					pRetBuff = Mem_BP;
					OSI_read(fileHandle, 0, fsize, Mem_BP);
				}
			}
			/*note: memory is consumed even when the file did not fit*/
			linearAddr += fsize;
			Mem_BP = NORM2PTR(linearAddr);
			Mem_Remain -= fsize;
			OSI_close(fileHandle);
		}

		return pRetBuff;
	}

	if(IsZFile) {
		pRetBuff = pBuffer;
		fileHandle = OSI_open(fname);
		if(fileHandle >= 0) {
			OSI_read(fileHandle, 0, 4, &fsize);
			/*work area: right after the data when loading into
			  ScratchBuf, else ScratchBuf itself*/
			if(pBuffer == ScratchBuf) {
				aSegment = FP_SEG(pBuffer) + ((FP_OFF(pBuffer) + fsize + 0xf) >> 4);
			} else {
				aSegment = FP_SEG(ScratchBuf);
				aSegment += ((FP_OFF(ScratchBuf) + 0xf) >> 4);
			}
			decompress(fileHandle, 0L, pBuffer, aSegment);
			OSI_close(fileHandle);
		}
	} else {
		pRetBuff = pBuffer;
		fileHandle = OSI_open(fname);
		OSI_read(fileHandle, 0, OSI_fileSize(fileHandle), pBuffer);
		OSI_close(fileHandle);
	}

	return pRetBuff;
}

/*file loader callback given to the graphic driver (tGr._24):
  same as the "pBuffer != 0" part of LoadFile, but the file name is a far
  string (copied locally) and the compression flag is a parameter.
  Returns nothing (pRetBuff is computed but unused)*/
/*C_0903_033A*/LoadFileGr(char far *fn, void far *pBuffer, int16 isZ) {
	int16 fileHandle, i;
	void far *pRetBuff;
	int32 fsize;
	uint16 aSegment;
	char fname[14];

	for(i = 0; i < 13; i++)
		fname[i] = fn[i];
	
	if(isZ) {
		pRetBuff = pBuffer;
		fileHandle = OSI_open(fname);
		if(fileHandle >= 0) {
			OSI_read(fileHandle, 0, 4, &fsize);
			if(pBuffer == ScratchBuf) {
				aSegment = FP_SEG(pBuffer) + ((FP_OFF(pBuffer) + fsize + 0xf) >> 4);
			} else {
				aSegment = FP_SEG(ScratchBuf);
				aSegment += ((FP_OFF(ScratchBuf) + 0xf) >> 4);
			}
			decompress(fileHandle, 0L, pBuffer, aSegment);
			OSI_close(fileHandle);
		}
	} else {
		pRetBuff = pBuffer;
		fileHandle = OSI_open(fname);
		OSI_read(fileHandle, 0, OSI_fileSize(fileHandle), pBuffer);
		OSI_close(fileHandle);
	}
}

/*OSI file error handler (installed in OSI_ErrorHandler by main, see OSI_FILE.ASM).
  Called by the OSI file functions with OSI_ErrorCode = DOS error code (-1 = fewer
  bytes read/written than asked) and OSI_ErrorFunc = function (1 open, 2 close,
  3 create, 4 delete, 5 read, 6 write, 7 rename, 8 copy).
  Leaving OSI_ErrorCode != 0 makes the operation fail, clearing it means "retry".
  A short write is reported as "disk full", other errors are displayed and
  a key is awaited: <CTRL+C>/<ESC> give up, any other key retries.
  Note: "key" is not initialized on the "silent" and "disk full" paths.*/
/*C_0903_0430*/void FileErrorHandler() {
	int16 key;
	struct tClipInfo *savedClip;

	/*end of file reached: not an error*/
	if(OSI_ErrorCode == -1 && OSI_ErrorFunc == 5/*OSI_read*/)
		if(IgnoreShortRead)
			return;
	/*deleting a file that does not exist: not an error*/
	if(OSI_ErrorCode == 2 && OSI_ErrorFunc == 4/*OSI_delete*/)
		return;
	if(ConsoleReady) {
		savedClip = CurClip;
		CON_setClip(&(TextWindows[3]));
		if(OSI_ErrorCode == -1 && OSI_ErrorFunc == 6/*OSI_write*/) {
			DISK_full();
		} else {
			CON_printf(/*0264*/"DISK ERROR!!!! Function: %d, error: %d, file: ", OSI_ErrorFunc, OSI_ErrorCode);
			CON_printf(OSI_ErrorFile);
			CON_printf("\n");
			/*wait for a key (flushing the keyboard buffer)*/
			while(!(key = KBD_GetKey(1)));
		}
		CON_setClip(savedClip);
	}
	/*not <CTRL+C>/<ESC>: retry*/
	if(key != 3 && key != 27)
		OSI_ErrorCode = 0;
}

/*give all the remaining memory to the graphic driver (which loads its
  tile graphics there through the callbacks; tGr._20 == 0 means failure),
  then load the tile animation table "animdata" and the tile/type
  property tables "tileflag"*/
/*C_0903_04DB*/static InitGraphicsData() {
	int16 animFile, flagFile;
	uint16 aSegment;
	int32 linearAddr;

	linearAddr = PTR2NORM(Mem_BP);
	if(linearAddr & 0xf) {
		Mem_Remain -= 0x10 - (linearAddr & 0xf);
		linearAddr = (linearAddr & ~0xf) + 0x10;
		Mem_BP = NORM2PTR(linearAddr);
	}
	aSegment = linearAddr >> 4;
	DISK_confirm(DISK_GAME);
	GR_INIT_MEMORY(aSegment, (uint16)(Mem_Remain >> 4));
	GrMemory = GrInfo->pDriverMem;
	if(GrMemory == 0) {
		OSI_delay_clean();
		GR_SHUTDOWN();
		exit(1);
	}
	GrMemorySeg = FP_SEG(GrMemory) + (((int32)GrMemory & 0xffff) >> 4);/*unused?*/

	/*animated tiles: count, tile number, first frame, frame mask, shift*/
	DISK_confirm(DISK_GAME);
	animFile = OSI_open(/*0295*/"animdata");
	OSI_read(animFile, 0, 2, &NumAnimData);
	OSI_read(animFile, -1, MAX_ANIM * sizeof(int16), AnimTile);
	OSI_read(animFile, -1, MAX_ANIM * sizeof(int16), AnimFirstFrame);
	OSI_read(animFile, -1, MAX_ANIM, AnimFrameMask);/*anim tile mask*/
	OSI_read(animFile, -1, MAX_ANIM, AnimFrameShift);/*anim tile shift*/
	OSI_close(animFile);

	/*0x800 tiles terrain flags, 0x800 tiles flags, 0x400 types weight,
	  0x800 tiles flags #2*/
	DISK_confirm(DISK_GAME);
	flagFile = OSI_open(/*029E*/"tileflag");
	OSI_read(flagFile, 0, 0x800, TerrainType);
	OSI_read(flagFile, -1, 0x800, TileFlag);
	OSI_read(flagFile, -1, 0x400, TypeWeight);
	OSI_read(flagFile, -1, 0x800, TileFlag2);
	OSI_close(flagFile);
}

/*display the main screen background "paper.bmp" (decompressed into the
  free far memory, which is not reserved)*/
/*C_0903_070B*/ShowMainScreen() {
	int16 fileHandle;
	struct tPixmap far *pixmap;

	DISK_confirm(DISK_GAME);
	pixmap = Mem_BP;
	fileHandle = OSI_open(BitmapFiles[0]);
	if(fileHandle < 0)
		return;
	/*work area after the 4 bytes header + 320x200 pixels*/
	decompress(fileHandle, 0L, pixmap, FP_SEG(pixmap) + (FP_OFF(pixmap) >> 4) + (0xfa20 >> 4));
	OSI_close(fileHandle);
	GR_DRAW_PIXMAP(pixmap, 0, 0);
}

/*VGA only: send the whole 256 colors palette VGAPalette to the DAC
  (BIOS int 10h/1012h); also bound to <ALT+P>*/
/*C_0903_0776*/SetVGAPalette() {
	union REGS regs;
	struct SREGS sregs;

	if(GrAdapter != 0)
		return;
	regs.x.ax = 0x1012;
	regs.x.bx = 0;
	regs.x.cx = 0x100;
	regs.x.dx = FP_OFF(VGAPalette);
	segread(&sregs);
	sregs.es = FP_SEG(VGAPalette);
	int86x(0x10, &regs, &regs, &sregs);
}

/*load and set the palette ("u6pal" for VGA, by the driver otherwise)*/
/*C_0903_07C4*/LoadPalette() {
	int16 fileHandle;

	if(GrAdapter == 0) {
		__MemAlloc(&VGAPalette, 0x100 * 3);
		DISK_confirm(DISK_GAME);
		fileHandle = OSI_open(/*02A7*/"u6pal");
		OSI_read(fileHandle, 0, 0x100 * 3, VGAPalette);
		OSI_close(fileHandle);
		SetVGAPalette();
	} else {
		GR_INIT_PALETTE();
	}
}

/*palette cycling for fire/water/lava colors (not while time is stopped).
  VGA: color 0xff pulses in grey levels 12..19, colors 0xe0-0xe7 and
  0xe8-0xef rotate by one step per call, colors 0xf0-0xf3, 0xf4-0xf7 and
  0xf8-0xfb every second call (values taken from the VGAPalette palette).
  Other adapters: done by the graphic driver*/
/*C_0903_0820*/PaletteAnimation() {
	int16 rgb;
	union REGS regs;
	int16 step;

	if(SpellFx[14] != 0)
		return;
	if(GrAdapter == 0) {
		/*triangle wave 0..7..0 -> grey level 12..19*/
		step = (PaletteCycle ++) & 0xf;
		if(step > 7)
			step = 0xf - step;
		step += 0xc;
		/*BIOS: set DAC register bx to (dh,ch,cl)*/
		regs.x.ax = 0x1010;
		regs.x.bx = 0xff;
		regs.h.dh = 
		regs.h.ch = 
		regs.h.cl = step;
		int86(0x10, &regs, &regs);
		/*0x3c8: DAC write index, 0x3c9: r,g,b*/
		for(step = 0; step < 8; step ++) {
			outportb(0x3c8, ((PaletteCycle - step) & 7) + 0xe0);
			for(rgb = 0; rgb < 3; rgb ++)
				outportb(0x3c9, VGAPalette[0xe7 - step].content[rgb]);

			outportb(0x3c8, ((PaletteCycle - step) & 7) + 0xe8);
			for(rgb = 0; rgb < 3; rgb ++)
				outportb(0x3c9, VGAPalette[0xef - step].content[rgb]);
		}
		for(step = 0; step < 4; step ++) {
			outportb(0x3c8, (((PaletteCycle >> 1) - step) & 3) + 0xf0);
			for(rgb = 0; rgb < 3; rgb ++)
				outportb(0x3c9, VGAPalette[0xf3 - step].content[rgb]);

			outportb(0x3c8, (((PaletteCycle >> 1) - step) & 3) + 0xf4);
			for(rgb = 0; rgb < 3; rgb ++)
				outportb(0x3c9, VGAPalette[0xf7 - step].content[rgb]);

			outportb(0x3c8, (((PaletteCycle >> 1) - step) & 3) + 0xf8);
			for(rgb = 0; rgb < 3; rgb ++)
				outportb(0x3c9, VGAPalette[0xfb - step].content[rgb]);
		}
	} else {
		GR_CYCLE_PALETTE((unsigned char)(PaletteCycle ++));
	}
}

/*build the configuration string given to END.EXE/ULTIMA6.EXE
  (same format as CONFIG.U6): adapter letter, 'M'ouse/'K'eyboard, music
  letter and, for AdLib/CMS, the 3 digits i/o port (digits above 9 would
  not come out as hex)*/
/*C_0903_09BB*/BuildConfigString(char *dst/*bp06*/) {
	*(dst++) = GrAdapterLetters[GrAdapter];
	*(dst++) = MouseOn?'M':'K';
	*(dst++) = MusicAdapterLetters[MusicAdapter];
	if(MusicAdapter == 1 || MusicAdapter == 2) {
		*(dst++) = (MusicPort >> 8) + '0';
		*(dst++) = ((MusicPort >> 4) & 0xf) + '0';
		*(dst++) = (MusicPort & 0xf) + '0';
	}
	*dst = 0;
}

/*the Codex has vanished: launch the endgame END.EXE with the time spent
  in Britannia since 4/7/161 (years, months, days) - never returns*/
/*C_0903_0A1E*/LaunchEndGame() {
	char param_str[8],y_str[6],m_str[6],d_str[6];
	int16 years,monthes,days;

	years = Date_Y - 161;
	monthes = Date_M - 7;
	days = Date_D - 4;
	/*bug: should test and fix "days" (Date_D is unsigned, so this never
	  happens and "days" may be negative); also months wrap at 12, not 13*/
	if(Date_D < 0/*should be "days"?bug?*/) {
		Date_D += 28;
		monthes --;
	}
	if(monthes < 0) {
		monthes += 13;
		years --;
	}
	CON_itoa(years, y_str, 5, 10);
	CON_itoa(monthes, m_str, 5, 10);
	CON_itoa(days, d_str, 5, 10);
	DISK_confirm(DISK_ENDGAME);
	MUS_PlaySong(0);
	OSI_delay_clean();
	GR_SHUTDOWN();
	BuildConfigString(param_str);
	do {
		execl(/*02AD*/"end.exe", /*02AD*/"end.exe", param_str, y_str, m_str, d_str, 0);
	} while(1);
}

/*parse an hexadecimal music i/o port (e.g. "A388") into MusicPort;
  returns the pointer to the first non hex digit*/
/*C_0903_0AE6*/char *ParseHexPort(unsigned char *str/*bp06*/) {
	int16 ch;

	for(MusicPort = 0; ((ch = toupper(*str)) >= '0' && ch <= '9') || (ch >= 'A' && ch <='F'); str ++) {
		MusicPort <<= 4;
		if(ch >= '0' && ch <= '9')
			MusicPort += ch - '0';
		else
			MusicPort += ch - 'A' + 10;
	}
	return str;
}

/*parse a configuration string (CONFIG.U6 or a command line argument):
  V/E/T/C/H graphic adapter, M/K mouse or keyboard, N/A/X/Y/R/Z/I music
  adapter (A, X, Z, I may be followed by an hex i/o port), '$' see GrDriverOption*/
/*C_0903_0B46*/cfg_parse(unsigned char *str/*bp06*/) {
	while(*str) {
		switch(toupper(*str++)) {
			case '$': GrDriverOption = 1; break;
			case 'V': GrAdapter = 0; break;
			case 'E': GrAdapter = 1; break;
			case 'T': GrAdapter = 2; break;
			case 'C': GrAdapter = 3; break;
			case 'H': GrAdapter = 4; break;
			case 'M': MouseOn = 1; break;
			case 'K': MouseOn = 0; break;
			case 'N': MusicAdapter = 0; break;
			case 'A': MusicAdapter = 1; MusicPort = 0x388;
				if(*str == '3' || *str == '2')
					str = ParseHexPort(str);
			break;
			case 'X': MusicAdapter = 2; MusicPort = 0x220;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
			case 'Y': MusicAdapter = 3; break;
			case 'R': MusicAdapter = 4; break;
			case 'Z': MusicAdapter = 5; MusicPort = 0x220;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
			case 'I': MusicAdapter = 6; MusicPort = 0x280;
				if(*str == '2')
					str = ParseHexPort(str);
			break;
		}
	}
}

/*mouse pointer shapes, by graphic adapter*/
/*01E1*/char *PointerFiles[] = {
	/*02B5*/"u6mcga.ptr",
	/*02C0*/"u6curs.ega",
	/*02CB*/"u6curs.tga",
	/*02D6*/"u6curs.cga",
	/*02D6*/"u6curs.cga"
};

/*program entry: initialization, game loop, exit*/
/*C_0903_0C5E*/cdecl main(int16 argc, char **argv) {
	int16 i, fileHandle;
	char cfgString[7];
	int16 hercRow;

	/*seed the random generator from the time of day*/
	OSI_randomize(OSI_timeSeed());
	/*config: CONFIG.U6 (6 characters) then the command line arguments*/
	fileHandle = OSI_open(/*02E1*/"config.u6");
	if(fileHandle >= 0) {
		OSI_read(fileHandle, 0, 6, cfgString);
		cfgString[6] = 0;
		cfg_parse(cfgString);
		OSI_close(fileHandle);
	}
	for(i = 1; i < argc; i ++)
		cfg_parse(argv[i]);
	/*memory: take the whole far heap, __MemAlloc() hands it out*/
	FarHeapSize = farcoreleft();
	FarHeap = farmalloc(FarHeapSize);
	if(!FarHeap)
		goto c_12ef;
	Mem_BP = FarHeap;
	Mem_Remain = FarHeapSize;

	DISK_init();
	OSI_lowInit();/*init LOW?*/
	OSI_delay_init();
	/*install the file error handler*/
	OSI_ErrorHandler.f = FileErrorHandler;
	/*the surfaces (scanline offset tables) are only used by the mouse code:
	  Screen = the 320x200 screen, MapSurface = the 160x160 map view*/
	if(MouseOn) {
		Screen.pScanlines = ScreenScanlines;
		Screen.left = 0;
		Screen.right = 319;
		Screen.top = 0;
		Screen.bottom = 199;

		MapSurface.pScanlines = MapScanlines;
		MapSurface.left = 0;
		MapSurface.right = 159;
		MapSurface.top = 0;
		MapSurface.bottom = 159;

		if(GrAdapter == 0) {/*VGA*/
			Screen.segment = 0xa000;
			ScreenScanlines[0] = 0;
			for(i = 1; i < 200; i++)
				ScreenScanlines[i] = ScreenScanlines[i - 1] + 0x140;
		} else
		if(GrAdapter == 1) {/*EGA*/
			Screen.segment = 0xa000;
			ScreenScanlines[0] = 0;
			for(i = 1; i < 200; i++)
				ScreenScanlines[i] = ScreenScanlines[i - 1] + 40;
			Screen.pScanlines = ScreenScanlines;
			MapScanlines[0] = 0x141;
			for(i = 1; i < 176; i++)
				MapScanlines[i] = MapScanlines[i - 1] + 40;
			MapSurface.segment = 0xa200;
		} else
		if(GrAdapter == 2) {/*TANDY*/
			Screen.segment = 0xb800;
			ScreenScanlines[0] = 0;
			ScreenScanlines[1] = 0x2000;
			ScreenScanlines[2] = 0x4000;
			ScreenScanlines[3] = 0x6000;
			for(i = 4; i < 200; i++)
				ScreenScanlines[i] = ScreenScanlines[i - 4] + 0xa0;
			Screen.pScanlines = ScreenScanlines;
		} else
		if(GrAdapter == 3) {/*CGA*/
			Screen.segment = 0xb800;
			ScreenScanlines[0] = 0;
			ScreenScanlines[1] = 0x2000;
			for(i = 2; i < 200; i++)
				ScreenScanlines[i] = ScreenScanlines[i - 2] + 80;
			Screen.pScanlines = ScreenScanlines;
		} else
		if(GrAdapter == 4) {/*HERCULES*/
			Screen.segment = 0xb000;
			/*200 lines stretched to 300 (x1.5), 4 interleaved banks
			  of 90 bytes per line*/
			for(i = 0; i < 200; i++) {
				hercRow = (i * 3 >> 1) + 0xf;
				ScreenScanlines[i] = ((hercRow & 3) << 13) + (hercRow >> 2) * 90 + 5;
			}
			Screen.pScanlines = ScreenScanlines;
		}
		/*background save area and pointer shapes*/
		__MemAlloc(&Mouse.saveBuf, 0x400);
		__MemAlloc(&PointerShapes, 0x800);
		DISK_confirm(DISK_GAME);
		fileHandle = OSI_open(PointerFiles[GrAdapter]);
		if(fileHandle >= 0) {
			decompress(fileHandle, 0L, PointerShapes, FP_SEG(Mem_BP) + 1);
			OSI_close(fileHandle);
		}
		Mouse.shapes = PointerShapes;
		Mouse.surface = &Screen;
		MOUSE_INIT(&Mouse);
		Mouse.onMove = CON_UpdateMousePointer;
		MOUSE_POINTER(PointerShapes, 0);
	}
	InitGraphicsDriver(GrAdapter);/*init gr*/
	MUS_InitDriver(MusicAdapter, MusicPort);/*init music*/
	LoadPalette();
	DISK_confirm(DISK_GAME);
	/*font*/
	GrInfo->font = LoadFile(/*02EB*/"U6.CH", 0L);
	ConsoleReady = 1;
	/*text windows: [0] whole screen, [1] status area, [3] message area*/
	CON_createClip(&(TextWindows[0]), 0, 0, 39, 24);
	ShowMainScreen();
	CON_createClip(&(TextWindows[1]), 22,  1, 38, 12);
	CON_createClip(&(TextWindows[3]), 22, 14, 38, 23);
	CON_setClip(&(TextWindows[3]));
	/*message area: word wrap, scroll, capitalize*/
	CurClip->attrFlags |= 1;
	CurClip->attrFlags |= 0x20;
	CurClip->attrFlags |= 4;

	__MemAlloc(&TerrainType, 0x800);
	__MemAlloc(&TileFlag, 0x800);
	__MemAlloc(&BaseTile, 0x800);
	__MemAlloc(&TypeWeight, 0x400);
	__MemAlloc(&TileFlag2, 0x800);

	/*ScratchBuf is also the 176x176 (0x7900 bytes) map view back buffer:
	  MapScanlines gives the offsets of its 160x160 inner part (8 pixels border)*/
	__MemAlloc(&ScratchBuf, 0x7900);
	GrInfo->pWorkBuf = ScratchBuf;
	if(GrAdapter == 0) {/*VGA*/
		MapSurface.segment = FP_SEG(ScratchBuf);
		MapScanlines[0] = FP_OFF(ScratchBuf) + 0x588;
		for(i = 1; i < 0xa0; i++)
			MapScanlines[i] = MapScanlines[i - 1] + 0xb0;
	} else
	if(GrAdapter == 2) {/*TANDY*/
		MapSurface.segment = FP_SEG(ScratchBuf);
		MapScanlines[0] = FP_OFF(ScratchBuf) + 0x404;
		for(i = 1; i < 0xa0; i++)
			MapScanlines[i] = MapScanlines[i - 1] + 0x80;
	} else
	if(GrAdapter == 3 || GrAdapter == 4) {/*CGA/HERCULES*/
		MapSurface.segment = FP_SEG(ScratchBuf);
		MapScanlines[0] = FP_OFF(ScratchBuf) + 0x282;
		for(i = 1; i < 0xa0; i++)
			MapScanlines[i] = MapScanlines[i - 1] + 0x50;
	}

	__MemAlloc(&Schedule, 600 * sizeof(struct tSchedule));
	__MemAlloc(&NPCMode, 0x100);
	__MemAlloc(&NPCComMode, 0x100);
	__MemAlloc(&SchedIndex, 0x100);
	__MemAlloc(&Leader, 0x100);
	__MemAlloc(&NPCFlag, 0x100);
	__MemAlloc(&OrigShapeType, 0x100 * sizeof(int16));
	__MemAlloc(&Level, 0x100);
	__MemAlloc(&PathData, 0x100);
	__MemAlloc(&ChunkIndexBuf, 0x600);

	__MemAlloc(&TalkBuf, 0x2800);

	OBJ_InitLists();

	InitGraphicsData();

	/*draw the command icons and the combat mode icon*/
	for(i = 0; i < 9; i ++)
		GR_PUT_TILE_SCREEN(TIL_BUTTON_ATTACK + i, TIL2SCR(i) + 8, 176);
	GR_PUT_TILE_SCREEN(TIL_BUTTON_COMBAT_OFF, 152, 176);
	/* */
	OSI_Started = 1;/*never read*/
	MAP_InitChunkCache();/*empty the map chunk cache*/
	GAME_Load();/*load game?*/
	/*set every animated tile to its first frame*/
	for(i = 0; i < NumAnimData; i ++)
		GR_SET_ANIM_FRAME(AnimTile[i], AnimFirstFrame[i]);

	/*no character in the saved game: go back to ULTIMA6.EXE*/
	if(AvatarPortrait == 0) {
		CON_printf(/*02F1*/"You must first create or transfer a character. ");
		AnimateAtPrompt = 0;
		CON_getch();
		DISK_confirm(DISK_PROGRAM);
		OSI_delay_clean();
		GR_SHUTDOWN();
		BuildConfigString(cfgString);
		while(1)
			execl(/*0321*/"ultima6.exe", /*0321*/"ultima6.exe", cfgString, 0);
	}

	InitNPCTracker();

	PassTime(0);/*refresh the time dependent state*/

	ShowMouse
	if(InCombat)
		COMBAT_begin();
	else
		MUS_PlayAreaSong();/*play a random song?*/

	/*-- game loop --*/
	GameLoop();
	/*-- --*/

	/*exit: stop the music, delete the temporary files*/
	MUS_PlaySong(0);
	OBJ_DeleteTmpBlocks();
	OSI_delay_clean();

c_12ef:
	/*restore the video mode*/
	GR_SHUTDOWN();
	exit(0);
}
