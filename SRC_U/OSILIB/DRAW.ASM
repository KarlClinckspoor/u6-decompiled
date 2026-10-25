;----------------------------------------
;OSI library (U.EXE): lines and pixels on a surface (struct tSurface, 1 byte a pixel)
;----------------------------------------
INCLUDE OSI.INC

DSegStart
;Bresenham's line
stepX	dw 0	;D_4592 +1/-1
stepY	dw 0	;D_4594 +1/-1
deltaX	dw 0	;D_4596 |x1 - x2|
deltaY	dw 0	;D_4598 |y1 - y2|
count	dw 0	;D_459A pixels left
error	dw 0	;D_459C
DSegEnd

CSegStart	DRAW_TEXT

C_0002	dw DGROUP

;draw a line, clipped to the window:
;DRW_line(struct tSurface *s, int x1, int y1, int x2, int y2, int color)
ProcStart	DRW_line	;12B7:0004
	MOV	AX,[BP+0Ah]
	CMP	AX,[BP+0Eh]
	JNZ	C_0065
	;-- a horizontal line
	MOV	SI,[BP+06]
	MOV	DX,[BP+08]
	MOV	CX,[BP+0Ch]
	CMP	DX,CX
	JLE	C_0021
	XCHG	DX,CX
C_0021:
	CMP	CX,[SI].left
	JL	C_0062
	CMP	CX,[SI].right
	JLE	C_002E
	MOV	CX,[SI].right
C_002E:
	CMP	DX,[SI].right
	JG	C_0062
	CMP	DX,[SI].left
	JGE	C_003B
	MOV	DX,[SI].left
C_003B:
	CMP	AX,[SI].top
	JL	C_0062
	CMP	AX,[SI].bottom
	JG	C_0062
	SUB	CX,DX
	INC	CX
	MOV	ES,[SI].segment_
	LDS	DI,[SI].pScanlines
	SHL	AX,1
	ADD	DI,AX
	MOV	DI,[DI]
	ADD	DI,DX
	MOV	AX,[BP+10h]
	MOV	AH,AL
	SHR	CX,1
	REP	STOSW
	RCL	CX,1
	REP	STOSB
C_0062:
	JMPN	C_015D
C_0065:	;-- any other line
	MOV	AX,[BP+08]
	SUB	AX,[BP+0Ch]
	MOV	SI,-1
	JNB	C_0074
	NEG	SI
	NEG	AX
C_0074:
	MOV	BX,[BP+0Ah]
	SUB	BX,[BP+0Eh]
	MOV	DI,-1
	JNB	C_0083
	NEG	DI
	NEG	BX
C_0083:
	MOV	stepX,SI
	MOV	stepY,DI
	MOV	deltaX,AX
	MOV	deltaY,BX
	MOV	SI,[BP+06]
	MOV	ES,[SI].segment_
	CMP	AX,BX
	JB	C_00FB
	;-- mostly horizontal
	MOV	count,AX
	SHR	AX,1
	MOV	error,AX
	MOV	AX,[BP+08]
	MOV	BX,[BP+0Ah]
C_00A9:
	CMP	AX,[SI].right
	JG	C_00D5
	CMP	AX,[SI].left
	JL	C_00D5
	CMP	BX,[SI].bottom
	JG	C_00D5
	CMP	BX,[SI].top
	JL	C_00D5
	LDS	DI,[SI].pScanlines
	SHL	BX,1
	ADD	DI,BX
	SHR	BX,1
	MOV	DI,[DI]
	ADD	DI,AX
	MOV	DX,[BP+10h]
	MOV	ES:[DI],DL
	MOV	DS,CS:C_0002
C_00D5:
	DEC	count
	JS	C_013B
	MOV	CX,deltaY
	ADD	error,CX
	MOV	CX,error
	SUB	CX,deltaX
	JBE	C_00F5
	MOV	error,CX
	ADD	BX,stepY
C_00F5:
	ADD	AX,stepX
	JMP	SHORT C_00A9
C_00FB:	;-- mostly vertical
	MOV	count,BX
	SHR	BX,1
	MOV	error,BX
	MOV	AX,[BP+08]
	MOV	BX,[BP+0Ah]
C_010B:
	CMP	AX,[SI].right
	JG	C_0137
	CMP	AX,[SI].left
	JL	C_0137
	CMP	BX,[SI].bottom
	JG	C_0137
	CMP	BX,[SI].top
	JL	C_0137
	LDS	DI,[SI].pScanlines
	SHL	BX,1
	ADD	DI,BX
	SHR	BX,1
	MOV	DI,[DI]
	ADD	DI,AX
	MOV	DX,[BP+10h]
	MOV	ES:[DI],DL
	MOV	DS,CS:C_0002
C_0137:
	DEC	count
C_013B:
	JS	C_015D
	MOV	CX,deltaX
	ADD	error,CX
	MOV	CX,error
	SUB	CX,deltaY
	JBE	C_0157
	MOV	error,CX
	ADD	AX,stepX
C_0157:
	ADD	BX,stepY
	JMP	SHORT C_010B
C_015D:
ProcEnd	DRW_line

;the points of a line, each given to a function:
;DRW_lineFunc(void (far *plot)(struct tSurface *s, int x, int y, int color),
;	struct tSurface *s, int x1, int y1, int x2, int y2, int color)
ProcStart	DRW_lineFunc	;12B7:0162
	MOV	AX,[BP+0Ch]
	SUB	AX,[BP+10h]
	MOV	SI,-1
	JNB	C_0177
	NEG	SI
	NEG	AX
C_0177:
	MOV	BX,[BP+0Eh]
	SUB	BX,[BP+12h]
	MOV	DI,-1
	JNB	C_0186
	NEG	DI
	NEG	BX
C_0186:
	MOV	stepX,SI
	MOV	stepY,DI
	MOV	deltaX,AX
	MOV	deltaY,BX
	MOV	SI,[BP+0Ah]
	MOV	ES,[SI].segment_
	CMP	AX,BX
	JB	C_01E3
	;-- mostly horizontal
	MOV	count,AX
	SHR	AX,1
	MOV	error,AX
	MOV	AX,[BP+0Ch]
	MOV	BX,[BP+0Eh]
C_01AC:
	PUSH	WORD PTR [BP+14h]
	PUSH	BX
	PUSH	AX
	PUSH	WORD PTR [BP+0Ah]
	CALL	DWORD PTR [BP+06]
	POP	BX
	POP	AX
	POP	BX
	ADD	SP,2
	DEC	count
	JS	C_022A
	MOV	CX,deltaY
	ADD	error,CX
	MOV	CX,error
	SUB	CX,deltaX
	JBE	C_01DD
	MOV	error,CX
	ADD	BX,stepY
C_01DD:
	ADD	AX,stepX
	JMP	SHORT C_01AC
C_01E3:	;-- mostly vertical
	MOV	count,BX
	SHR	BX,1
	MOV	error,BX
	MOV	AX,[BP+0Ch]
	MOV	BX,[BP+0Eh]
C_01F3:
	PUSH	WORD PTR [BP+14h]
	PUSH	BX
	PUSH	AX
	PUSH	WORD PTR [BP+0Ah]
	CALL	DWORD PTR [BP+06]
	POP	BX
	POP	AX
	POP	BX
	ADD	SP,2
	DEC	count
	JS	C_022A
	MOV	CX,deltaX
	ADD	error,CX
	MOV	CX,error
	SUB	CX,deltaY
	JBE	C_0224
	MOV	error,CX
	ADD	AX,stepX
C_0224:
	ADD	BX,stepY
	JMP	SHORT C_01F3
C_022A:
ProcEnd	DRW_lineFunc

;the color of a pixel, -1 outside the window: DRW_getPixel(struct tSurface *s, int x, int y)
ProcStart	DRW_getPixel	;12B7:022F
	MOV	AX,-1
	MOV	BX,[BP+06]
	MOV	DI,[BP+0Ah]
	CMP	DI,[BX].bottom
	JG	C_0264
	CMP	DI,[BX].top
	JL	C_0264
	MOV	SI,[BP+08]
	CMP	SI,[BX].left
	JL	C_0264
	CMP	SI,[BX].right
	JG	C_0264
	LDS	AX,[BX].pScanlines
	SHL	DI,1
	ADD	DI,AX
	ADD	SI,[DI]
	MOV	DS,[BX].segment_
	MOV	AL,[SI]
	MOV	AH,0
C_0264:
ProcEnd	DRW_getPixel

CSegEnd	DRAW_TEXT

END
