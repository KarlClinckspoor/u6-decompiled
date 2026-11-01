;----------------------------------------
;OSI library (U.EXE): gotoxy() for the text window
;(takes the place of CM.LIB's GOTOXY, code and all: see CLREOL.ASM. gotoxy() has no
;relocation, so nothing in U.EXE tells whether Origin split it the same way as clreol())
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN _pText:WORD	;D_45B2 the current text window
DSegEnd

CSegStart	GOTOXY_TEXT

;gotoxy(x, y): move the text window's cursor
ProcStart	_gotoxy	;115E:0004
	MOV	BX,_pText
	MOV	AX,[BP+06]
	MOV	[BX].curX,AX
	MOV	AX,[BP+08]
	MOV	[BX].curY,AX
ProcEnd	_gotoxy

CSegEnd	GOTOXY_TEXT

END
