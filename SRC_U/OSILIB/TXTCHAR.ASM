;----------------------------------------
;OSI library (U.EXE): text window, printing a character
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN _pText:WORD	;struct tText *: the current text window
EXTRN drvPutChar:DWORD	;driver: draw a character

theChar	dw 0	;D_45D0
charWidth	dw 0	;D_45D2
charHeight	dw 0	;D_45D4
twiceY	dw 0	;D_45D6 2 * the cursor's y (an offset in the scanline table)
DSegEnd

EXTRN TXT_scrollUp1:FAR	;TXT_scrollUp1

CSegStart	TXTCHAR_TEXT

;print a character: TXT_putc(int c)
ProcStart	_TXT_putc	;135A:0008
	MOV	AX,[BP+06]
	CALL	FAR PTR TXT_putcAL
ProcEnd	_TXT_putc

;add a character to the text window's string buffer: TXT_bufc(int c)
ProcStart	_TXT_bufc	;135A:001B
	MOV	AX,[BP+06]
	CALL	FAR PTR TXT_bufcAL
ProcEnd	_TXT_bufc

;the same with the character in AL
FProc	TXT_bufcAL	;135A:002E
	MOV	BX,_pText
	MOV	SI,[BX].bufPtr
	INC	[BX].bufPtr
	MOV	[SI],AL
	MOV	AL,0
	MOV	[SI+01],AL
	RET
TXT_bufcAL	ENDP

;print the character in AL at the cursor (13: carriage return, 10: new line), going to
;a new line when it does not fit
FProc	TXT_putcAL	;135A:0040
	MOV	AH,0
	CMP	AX,0Dh
	JNZ	C_004C
	JMPF	TXT_cr
C_004C:
	CMP	AX,0Ah
	JNZ	C_0056
	JMPF	TXT_newLine
C_0056:
	MOV	theChar,AX
	MOV	BX,_pText
	LES	SI,[BX].font
	MOV	BP,AX
	MOV	AX,ES:[SI]
	MOV	charHeight,AX
	MOV	AL,ES:[BP+SI+04]
	MOV	AH,0
	MOV	charWidth,AX
C_0071:
	CLD
	MOV	BX,_pText
	LES	SI,[BX].font
	MOV	AX,[BX].curY
	SHL	AX,1
	MOV	twiceY,AX
	MOV	AX,[BX].curX
	ADD	AX,charWidth
	DEC	AX
	MOV	SI,[BX].surface
	CMP	AX,[SI].right
	JLE	C_009A
	CALL	FAR PTR TXT_newLine
	MOV	AX,theChar
	JMP	SHORT C_0071
C_009A:
	MOV	AX,theChar
	MOV	BX,_pText
	MOV	CX,charHeight
	MOV	DX,charWidth
	MOV	SI,twiceY
	CALL	drvPutChar
	RET
TXT_putcAL	ENDP

;carriage return
FProc	TXT_cr	;135A:00B2
	MOV	BX,_pText
	MOV	SI,[BX].surface
	MOV	AX,[SI].left
	MOV	[BX].curX,AX
	RET
TXT_cr	ENDP

;new line, scrolling the window up at its bottom
FProc	TXT_newLine	;135A:00BF
	CALL	TXT_cr
	LES	DI,[BX].font
	MOV	AX,[BX].curY
	ADD	AX,ES:[DI]
	CMP	AX,[SI].bottom
	JL	C_00D7
	JMP	TXT_scrollUp1
C_00D7:
	MOV	AX,[BX].curY
	ADD	AX,ES:[DI]
	MOV	[BX].curY,AX
	RET
TXT_newLine	ENDP

CSegEnd	TXTCHAR_TEXT

END
