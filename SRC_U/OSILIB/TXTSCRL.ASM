;----------------------------------------
;OSI library (U.EXE): text window, scrolling and clearing a line
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN D_45B2:WORD	;struct tText *: the current text window
EXTRN D_45CC:DWORD	;driver: fill a rectangle (struct tSurface *s, int color)

lineRect	tSurface <0,0,0,0,0,0>	;D_45A4 the rectangle cleared by TXT_clearLine
DSegEnd

CSegStart	TXTSCRL_TEXT

C_0008	dw 0	;the scanlines' segment, while DS is the surface's
C_000A	dw DGROUP

;scroll the window up by n lines of text: TXT_scrollUp(int n)
ProcStart	TXT_scrollUp	;1317:000C
	MOV	AX,[BP+06]
C_0015:
	PUSH	AX
	CALL	FAR PTR TXT_scrollUp1
	POP	AX
	DEC	AX
	JNZ	C_0015
ProcEnd	TXT_scrollUp

;scroll the window down by n lines of text: TXT_scrollDown(int n)
ProcStart	TXT_scrollDown	;1317:0024
	MOV	AX,[BP+06]
C_002D:
	PUSH	AX
	CALL	FAR PTR TXT_scrollDown1
	POP	AX
	DEC	AX
	JNZ	C_002D
ProcEnd	TXT_scrollDown

;scroll up by one line of text, and clear the line at the bottom
FProc	TXT_scrollUp1	;1317:003C
	MOV	BX,D_45B2
	MOV	SI,[BX].surface
	LES	DI,[BX].font
	MOV	AX,[SI].top
	ADD	AX,ES:[DI]	;+ font height
	MOV	DX,[SI].bottom
C_004E:
	CMP	AX,DX
	JG	C_005F
	MOV	BX,AX
	SUB	BX,ES:[DI]
	PUSH	AX
	CALL	C_00F7
	POP	AX
	INC	AX
	JMP	SHORT C_004E
C_005F:
	MOV	BX,D_45B2
	MOV	SI,[BX].surface
	MOV	AX,[SI].bottom
	LES	DI,[BX].font
	SUB	AX,ES:[DI]
	MOV	DX,[SI].left
	INC	AX
	CALL	FAR PTR TXT_clearLine
	RET
TXT_scrollUp1	ENDP

;scroll down by one line of text, and clear the line at the top
FProc	TXT_scrollDown1	;1317:0078
	MOV	BX,D_45B2
	LES	DI,[BX].font
	MOV	SI,[BX].surface
	MOV	AX,[SI].bottom
	SUB	AX,ES:[DI]
	MOV	DX,[SI].top
C_008A:
	CMP	AX,DX
	JL	C_009F
	MOV	BX,AX
	ADD	BX,ES:[DI]
	PUSH	AX
	CALL	C_00F7
	POP	AX
	CMP	AX,DX
	JZ	C_009F
	DEC	AX
	JMP	SHORT C_008A
C_009F:
	MOV	BX,D_45B2
	MOV	SI,[BX].surface
	MOV	AX,[SI].top
	MOV	DX,[SI].left
	CALL	FAR PTR TXT_clearLine
	RET
TXT_scrollDown1	ENDP

;clear a line of text from (DX,AX) to the right of the window, in the background color
FProc	TXT_clearLine	;1317:00B1
	PUSH	BX
	MOV	SI,D_45B2
	MOV	DI,[SI].surface
	MOV	SI,OFFSET DGROUP:lineRect
	MOV	BX,[DI].segment_
	MOV	[SI].segment_,BX
	MOV	BX,WORD PTR [DI].pScanlines
	MOV	WORD PTR [SI].pScanlines,BX
	MOV	BX,WORD PTR [DI].pScanlines+2
	MOV	WORD PTR [SI].pScanlines+2,BX
	MOV	[SI].top,AX
	MOV	[SI].left,DX
	MOV	CX,[DI].right
	MOV	[SI].right,CX
	MOV	DI,D_45B2
	MOV	AL,[DI].bgColor
	XOR	AH,AH
	PUSH	AX
	LES	BX,[DI].font
	MOV	DX,ES:[BX]
	ADD	DX,[SI].top
	MOV	[SI].bottom,DX
	PUSH	SI
	CALL	D_45CC
	ADD	SP,4
	POP	BX
	RET
TXT_clearLine	ENDP

;copy scanline AX of the window to scanline BX (from its left to its right)
;(called near, but it returns with a RETF: it is inside a FAR procedure)
TXT_copyLine	PROC FAR
C_00F7:
	PUSH	DX
	CLD
	MOV	SI,D_45B2
	MOV	DI,[SI].surface
	MOV	ES,[DI].segment_
	MOV	CX,[DI].right
	SUB	CX,[DI].left
	INC	CX
	MOV	DX,WORD PTR [DI].pScanlines+2
	MOV	CS:C_0008,DX
	MOV	DX,[DI].left
	MOV	SI,WORD PTR [DI].pScanlines
	SHL	AX,1
	ADD	SI,AX
	MOV	DI,WORD PTR [DI].pScanlines
	SHL	BX,1
	ADD	DI,BX
	MOV	DS,CS:C_0008
	MOV	SI,[SI]
	MOV	DI,[DI]
	ADD	SI,DX
	ADD	DI,DX
	PUSH	ES
	POP	DS
	SHR	CX,1
	REP	MOVSW
	RCL	CX,1
	REP	MOVSB
	MOV	DS,CS:C_000A
	MOV	BX,D_45B2
	LES	DI,[BX].font
	POP	DX
	RET
TXT_copyLine	ENDP

CSegEnd	TXTSCRL_TEXT

END
