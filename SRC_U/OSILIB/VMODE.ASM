;----------------------------------------
;OSI library (U.EXE): BIOS video mode
;----------------------------------------
INCLUDE OSI.INC

CSegStart	VMODE_TEXT

;the current video mode: VID_getMode(void)
ProcStart	VID_getMode	;1336:000C
	MOV	AX,0Fh	;get current video mode
	INT	10h
	MOV	AH,0
ProcEnd	VID_getMode

;VID_setMode(int mode)
ProcStart	VID_setMode	;1336:001E
	MOV	AX,[BP+06]
	MOV	AH,0	;set video mode
	INT	10h
ProcEnd	VID_setMode

CSegEnd	VMODE_TEXT

END
