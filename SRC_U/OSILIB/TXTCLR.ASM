;----------------------------------------
;OSI library (U.EXE): text window, clearing
;----------------------------------------
;TASM fixup order: 0047 001B 002B   (TASM wrote these fixups in this order: tools/u/tasmorder.py)
INCLUDE OSI.INC

DSegStart
EXTRN _pText:WORD	;struct tText *: the current text window
DSegEnd

EXTRN TXT_clearLine:FAR	;clear a line of text from (DX,AX) to the right of the window

;clreol() for the text window, in the segment of Turbo C's clreol(): CM.LIB's CLREOL
;module is only the name _clreol at the start of this segment (see CLREOL.ASM)
CSegStart	CLREOL_TEXT

;clreol(): clear from the cursor to the end of its line
_clreol	PROC FAR	;10E1:000A
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
	MOV	BX,_pText
	MOV	DX,[BX].curX
	MOV	AX,[BX].curY
	CALL	TXT_clearLine
ProcEnd	_clreol

CSegEnd	CLREOL_TEXT

CSegStart	TXTCLR_TEXT

;clear the whole window and put the cursor at its top left
ProcStart	_TXT_clear	;1288:0002
	MOV	BX,_pText
	MOV	SI,[BX].surface
	MOV	AX,[SI].left
	MOV	[BX].curX,AX
	MOV	AX,[SI].top
	MOV	[BX].curY,AX
	CALL	FAR PTR TXT_clrEos
ProcEnd	_TXT_clear

;clear from the cursor to the end of the window
ProcStart	_TXT_clearEos	;1288:0024
	CALL	FAR PTR TXT_clrEos
ProcEnd	_TXT_clearEos

;the same, without a frame: one line of text after the other
FProc	TXT_clrEos	;1288:0034
	MOV	BX,_pText
	MOV	AX,[BX].curY
	MOV	DX,[BX].curX
C_003E:
	MOV	BX,[BX].surface
	CMP	AX,[BX].bottom
	JGE	C_005B
	PUSH	AX
	CALL	TXT_clearLine
	POP	AX
	MOV	DX,[BX].left
	MOV	BX,_pText
	LES	DI,[BX].font
	ADD	AX,ES:[DI]	;+ font height
	JMP	SHORT C_003E
C_005B:
	RET
TXT_clrEos	ENDP

CSegEnd	TXTCLR_TEXT

END
