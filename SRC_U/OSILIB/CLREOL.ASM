;----------------------------------------
;OSI library (U.EXE): clreol() for the text window
;Origin's CM.LIB had Turbo C's conio clreol() and gotoxy() replaced by versions that work
;on the OSI text window (U.EXE's main menu calls them as gotoxy and clreol).
;This module takes the place of CM.LIB's CLREOL (see tools/u/ubuild.py), and it is only
;a name: the code of clreol() is in the CLREOL_TEXT segment of TXTCLR.ASM. Two things
;in U.EXE say so: the code is where Turbo C's CLREOL module is laid out (so a module of
;CM.LIB starts the segment), and its relocation is in the table among those of TXTCLR,
;after SURFACE's (so TLINK met its fixup in OSILIB's TXTCLR module).
;----------------------------------------

CLREOL_TEXT	SEGMENT PUBLIC WORD 'CODE'
PUBLIC	_clreol
_clreol	LABEL	FAR	;10E1:000A: the code is TXTCLR.ASM's, which TLINK puts right here
CLREOL_TEXT	ENDS

END
