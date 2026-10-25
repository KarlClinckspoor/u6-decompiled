;----------------------------------------
;OSI library (U.EXE): text window, clearing
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN D_45B2:WORD	;struct tText *: the current text window
DSegEnd

EXTRN C_1317_00B1:FAR	;clear a line of text from (DX,AX) to the right of the window

CSegStart	TXTCLR_TEXT

;clear the whole window and put the cursor at its top left
ProcStart	TXT_clear	;1288:0002
	MOV	BX,D_45B2
	MOV	SI,[BX].surface
	MOV	AX,[SI].left
	MOV	[BX].curX,AX
	MOV	AX,[SI].top
	MOV	[BX].curY,AX
	CALL	FAR PTR TXT_clrEos
ProcEnd	TXT_clear

;clear from the cursor to the end of the window
ProcStart	TXT_clearEos	;1288:0024
	CALL	FAR PTR TXT_clrEos
ProcEnd	TXT_clearEos

;the same, without a frame: one line of text after the other
FProc	TXT_clrEos	;1288:0034
	MOV	BX,D_45B2
	MOV	AX,[BX].curY
	MOV	DX,[BX].curX
C_003E:
	MOV	BX,[BX].surface
	CMP	AX,[BX].bottom
	JGE	C_005B
	PUSH	AX
	CALL	C_1317_00B1
	POP	AX
	MOV	DX,[BX].left
	MOV	BX,D_45B2
	LES	DI,[BX].font
	ADD	AX,ES:[DI]	;+ font height
	JMP	SHORT C_003E
C_005B:
	RET
TXT_clrEos	ENDP

CSegEnd	TXTCLR_TEXT

END
