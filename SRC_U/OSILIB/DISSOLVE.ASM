;----------------------------------------
;OSI library (U.EXE): dissolving a picture into another, the pixels copied in a
;pseudo-random order (a maximal-length linear feedback shift register over the
;window's x and y bits), for each video mode
;----------------------------------------
;TASM fixup order: 0019 0000   (TASM wrote these fixups in this order: tools/u/tasmorder.py)
INCLUDE OSI.INC

DSegStart
srcRight	dw 0	;D_4612 the windows of the two surfaces
srcLeft	dw 0	;D_4614
srcTop	dw 0	;D_4616 (then 2 * top: an offset in the scanline table)
srcBottom	dw 0	;D_4618
dstRight	dw 0	;D_461A
dstLeft	dw 0	;D_461C
dstTop	dw 0	;D_461E
dstBottom	dw 0	;D_4620
srcSeg	dw 0	;D_4622 their segments
dstSeg	dw 0	;D_4624
srcLines	dw 200 dup(0)	;D_4626 their scanline tables
dstLines	dw 200 dup(0)	;D_47B6
winWidth	dw 0	;D_4946 of the window
winHeight	dw 0	;D_4948
lfsr	dw 0,0	;D_494A the shift register
taps	dw 0,0	;D_494E its feedback
xMask	dw 0	;D_4952 its low bits: x
yShift	dw 0	;D_4954 its bits above: y
count	dw 0	;D_4956 pixels left
perWait	dw 0	;D_4958 pixels between two waits
bitMask	db 80h,40h,20h,10h,08h,04h,02h,01h	;D_495A 8 pixels a byte
mask4	db 0C0h,30h,0Ch,03h	;D_4962 4 pixels a byte
tapTable	LABEL WORD	;D_4966 the feedback of an n-bit register at 4*n (n >= 2)
nmask4	db 3Fh,0CFh,0F3h,0FCh
mask2	db 0F0h,0Fh	;D_496A 2 pixels a byte
nmask2	db 0Fh,0F0h	;D_496C
	dd 3h,6h,0Ch,14h,30h,60h,0B8h,110h,240h,500h,0CA0h,1B00h,3500h,6000h,0B400h
	dd 12000h,20400h,72000h,90000h,140000h,300000h,420000h,0D80000h,1200000h
	dd 3880000h,7200000h,9000000h,14000000h,32800000h,48000000h,0A3000000h
DSegEnd

EXTRN _VID_waitRetrace:FAR	;wait for the vertical retrace

CSegStart	DISSOLVE_TEXT

C_000A	dw DGROUP

;dissolve a surface into another, one pixel at a time in a random order (a linear
;feedback shift register), 1 byte a pixel (MCGA):
;DIS_dissolve(struct tSurface *from, struct tSurface *to, int speed)
ProcStart	_DIS_dissolve	;13F2:000C
	MOV	AX,[BP+0Ah]	;-- a wait every (pixels / (7 * speed + 1)) pixels
	MOV	CL,3
	SHL	AX,CL
	SUB	AX,[BP+0Ah]
	INC	AX
	MOV	perWait,AX
	CALL	FAR PTR DIS_setup
	MOV	AX,srcRight
	SUB	AX,srcLeft
	INC	AX
	MOV	winWidth,AX
	MOV	DX,srcBottom
	SUB	DX,srcTop
	INC	DX
	MOV	winHeight,DX
	SHL	srcTop,1
	SHL	dstTop,1
	MOV	lfsr,3	;-- the register: as many bits as x and y need (at least 2)
	MOV	lfsr+2,0
	MOV	xMask,1
	MOV	yShift,1
	MOV	CX,2
C_0060:
	SHR	AX,1
	JZ	C_0079
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	STC	
	RCL	xMask,1
	INC	yShift
	INC	CX
	JMP	SHORT C_0060
C_0079:
	SHR	DX,1
	JZ	C_0089
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	INC	CX
	JMP	SHORT C_0079
C_0089:
	MOV	DX,winHeight
	MOV	AX,winWidth
	MUL	DX
	DEC	AX
	MOV	count,AX
	MOV	DX,0
	DIV	perWait
	MOV	perWait,AX
	MOV	BP,AX
	MOV	BX,CX
	SHL	BX,1
	SHL	BX,1
	MOV	AX,tapTable[BX]
	MOV	taps,AX
	MOV	AX,tapTable+2[BX]
	MOV	taps+2,AX
C_00B6:	;-- the next value of the register: a pixel, when in the window
	MOV	DX,lfsr+2
	MOV	AX,lfsr
	SHR	DX,1
	RCR	AX,1
	JNB	C_00CB
	XOR	DX,taps+2
	XOR	AX,taps
C_00CB:
	MOV	lfsr+2,DX
	MOV	lfsr,AX
	MOV	SI,AX
	AND	SI,xMask
	MOV	DI,AX
	MOV	CX,yShift
C_00DE:
	SHR	DX,1
	RCR	DI,1
	LOOP	C_00DE
	CMP	SI,winWidth
	JNB	C_00B6
	CMP	DI,winHeight
	JNB	C_00B6
	SHL	DI,1
	MOV	BX,OFFSET DGROUP:srcLines
	ADD	BX,srcTop
	ADD	BX,DI
	MOV	BX,[BX]
	ADD	BX,srcLeft
	MOV	ES,srcSeg
	MOV	CL,ES:[BX+SI]
	MOV	BX,OFFSET DGROUP:dstLines
	ADD	BX,dstTop
	ADD	BX,DI
	MOV	BX,[BX]
	ADD	BX,dstLeft
	MOV	ES,dstSeg
	MOV	ES:[BX+SI],CL
	DEC	BP
	JNZ	C_012A
	CALL	_VID_waitRetrace
	MOV	BP,perWait
C_012A:
	DEC	count
	JZ	C_0132
	JMP	SHORT C_00B6
C_0132:	;-- (0,0), which the register never gives
	MOV	BX,srcTop
	ADD	BX,OFFSET DGROUP:srcLines
	MOV	BX,[BX]
	ADD	BX,srcLeft
	MOV	ES,srcSeg
	MOV	CL,ES:[BX]
	MOV	BX,dstTop
	ADD	BX,OFFSET DGROUP:dstLines
	MOV	BX,[BX]
	ADD	BX,dstLeft
	MOV	ES,dstSeg
	MOV	ES:[BX],CL
ProcEnd	_DIS_dissolve

;the two surfaces of the caller's [BP+6], [BP+8]
FProc	DIS_setup	;13F2:0161
	CLD	
	MOV	BX,[BP+06]
	MOV	BP,[BP+08]
	MOV	AX,[BX]
	MOV	srcSeg,AX
	MOV	AX,DS:[BP]
	MOV	dstSeg,AX
	PUSH	DS
	POP	ES
	MOV	DI,OFFSET DGROUP:srcLines
	MOV	SI,[BX+02]
	MOV	DS,[BX+04]
	MOV	CX,0C8h
	REP	MOVSW
	MOV	DS,CS:C_000A
	MOV	DI,OFFSET DGROUP:dstLines
	MOV	SI,DS:[BP+02]
	MOV	DS,DS:[BP+04]
	MOV	CX,0C8h
	REP	MOVSW
	MOV	DS,CS:C_000A
	MOV	SI,BX
	ADD	SI,6
	MOV	DI,OFFSET DGROUP:srcRight
	MOV	CX,4
	REP	MOVSW
	MOV	SI,BP
	ADD	SI,6
	MOV	DI,OFFSET DGROUP:dstRight
	MOV	CX,4
	REP	MOVSW
	RET
DIS_setup	ENDP

;DIS_farcpy(void far *from, void far *to, unsigned n)
ProcStart	_DIS_farcpy	;13F2:01B9
	MOV	CX,[BP+0Eh]
	LES	DI,[BP+0Ah]
	LDS	SI,[BP+06]
	SHR	CX,1
	REP	MOVSW
	RCL	CX,1
	REP	MOVSB
ProcEnd	_DIS_farcpy

;the same for the EGA (16 colors, 4 planes, 8 pixels a byte)
ProcStart	_DIS_dissolveEGA	;13F2:01D5
	MOV	DX,3C4h
	MOV	AX,0F02h
	OUT	DX,AX
	MOV	DX,3CEh
	MOV	AX,205h
	OUT	DX,AX
	MOV	AX,[BP+0Ah]
	MOV	perWait,AX
	CALL	FAR PTR DIS_setup
	MOV	AX,srcRight
	SUB	AX,srcLeft
	INC	AX
	MOV	winWidth,AX
	MOV	DX,srcBottom
	SUB	DX,srcTop
	INC	DX
	MOV	winHeight,DX
	SHL	srcTop,1
	SHL	dstTop,1
	MOV	lfsr,3
	MOV	lfsr+2,0
	MOV	xMask,1
	MOV	yShift,1
	MOV	CX,2
C_022F:
	SHR	AX,1
	JZ	C_0248
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	STC	
	RCL	xMask,1
	INC	yShift
	INC	CX
	JMP	SHORT C_022F
C_0248:
	SHR	DX,1
	JZ	C_0258
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	INC	CX
	JMP	SHORT C_0248
C_0258:
	MOV	DX,winHeight
	MOV	AX,winWidth
	MUL	DX
	DEC	AX
	MOV	count,AX
	MOV	BP,perWait
	MOV	BX,CX
	SHL	BX,1
	SHL	BX,1
	MOV	AX,tapTable[BX]
	MOV	taps,AX
	MOV	AX,tapTable+2[BX]
	MOV	taps+2,AX
C_027D:
	MOV	DX,lfsr+2
	MOV	AX,lfsr
	SHR	DX,1
	RCR	AX,1
	JNB	C_0292
	XOR	DX,taps+2
	XOR	AX,taps
C_0292:
	MOV	lfsr+2,DX
	MOV	lfsr,AX
	MOV	SI,AX
	AND	SI,xMask
	MOV	DI,AX
	MOV	CX,yShift
C_02A5:
	SHR	DX,1
	RCR	DI,1
	LOOP	C_02A5
	CMP	SI,winWidth
	JNB	C_027D
	CMP	DI,winHeight
	JNB	C_027D
	SHL	DI,1
	MOV	BX,OFFSET DGROUP:srcLines
	ADD	BX,srcTop
	ADD	BX,DI
	MOV	BX,[BX]
	ADD	SI,srcLeft
	MOV	DI,SI
	SHR	DI,1
	SHR	DI,1
	SHR	DI,1
	ADD	DI,BX
	db	83h,0E6h,07h	;AND SI,7
	MOV	CH,bitMask[SI]
	MOV	ES,srcSeg
	MOV	DX,3CEh
	XOR	BL,BL
	MOV	AX,304h
C_02E5:
	OUT	DX,AX
	MOV	BH,ES:[DI]
	AND	BH,CH
	NEG	BH
	ROL	BX,1
	DEC	AH
	JGE	C_02E5
	MOV	AL,8
	MOV	AH,CH
	OUT	DX,AX
	MOV	ES,dstSeg
	MOV	BH,ES:[DI]
	MOV	ES:[DI],BL
	DEC	BP
	JNZ	C_030C
	CALL	waitTick
	MOV	BP,perWait
C_030C:
	DEC	count
	JZ	C_0315
	JMPN	C_027D
C_0315:
	MOV	BX,OFFSET DGROUP:srcLines
	ADD	BX,srcTop
	MOV	BX,[BX]
	MOV	SI,srcLeft
	MOV	DI,SI
	SHR	DI,1
	SHR	DI,1
	SHR	DI,1
	ADD	DI,BX
	db	83h,0E6h,07h	;AND SI,7
	MOV	CH,bitMask[SI]
	MOV	ES,srcSeg
	MOV	DX,3CEh
	XOR	BL,BL
	MOV	AX,304h
C_033F:
	OUT	DX,AX
	MOV	BH,ES:[DI]
	AND	BH,CH
	NEG	BH
	ROL	BX,1
	DEC	AH
	JGE	C_033F
	MOV	AL,8
	MOV	AH,CH
	OUT	DX,AX
	MOV	ES,dstSeg
	MOV	BH,ES:[DI]
	MOV	ES:[DI],BL
	MOV	DX,3CEh
	MOV	AX,5
	OUT	DX,AX
	MOV	AX,0FF08h
	OUT	DX,AX
ProcEnd	_DIS_dissolveEGA

;wait for the next timer tick
waitTick	PROC NEAR	;13F2:036C
	XOR	AH,AH
	INT	1Ah
	INC	DX
	MOV	BX,DX
C_0373:
	INT	1Ah
	CMP	DX,BX
	JNZ	C_0373
	RET
waitTick	ENDP

;the same for the CGA (4 colors, 4 pixels a byte)
ProcStart	_DIS_dissolveCGA	;13F2:037A
	MOV	AX,[BP+0Ah]
	MOV	perWait,AX
	CALL	FAR PTR DIS_setup
	MOV	AX,srcRight
	SUB	AX,srcLeft
	INC	AX
	MOV	winWidth,AX
	MOV	DX,srcBottom
	SUB	DX,srcTop
	INC	DX
	MOV	winHeight,DX
	SHL	srcTop,1
	SHL	dstTop,1
	MOV	lfsr,3
	MOV	lfsr+2,0
	MOV	xMask,1
	MOV	yShift,1
	MOV	CX,2
C_03C6:
	SHR	AX,1
	JZ	C_03DF
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	STC	
	RCL	xMask,1
	INC	yShift
	INC	CX
	JMP	SHORT C_03C6
C_03DF:
	SHR	DX,1
	JZ	C_03EF
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	INC	CX
	JMP	SHORT C_03DF
C_03EF:
	MOV	DX,winHeight
	MOV	AX,winWidth
	MUL	DX
	DEC	AX
	MOV	count,AX
	MOV	BP,perWait
	MOV	BX,CX
	SHL	BX,1
	SHL	BX,1
	MOV	AX,tapTable[BX]
	MOV	taps,AX
	MOV	AX,tapTable+2[BX]
	MOV	taps+2,AX
C_0414:
	MOV	DX,lfsr+2
	MOV	AX,lfsr
	SHR	DX,1
	RCR	AX,1
	JNB	C_0429
	XOR	DX,taps+2
	XOR	AX,taps
C_0429:
	MOV	lfsr+2,DX
	MOV	lfsr,AX
	MOV	SI,AX
	AND	SI,xMask
	MOV	DI,AX
	MOV	CX,yShift
C_043C:
	SHR	DX,1
	RCR	DI,1
	LOOP	C_043C
	CMP	SI,winWidth
	JNB	C_0414
	CMP	DI,winHeight
	JNB	C_0414
	SHL	DI,1
	MOV	BX,OFFSET DGROUP:srcLines
	ADD	BX,srcTop
	ADD	BX,DI
	MOV	BX,[BX]
	PUSH	SI
	ADD	SI,srcLeft
	PUSH	SI
	SHR	SI,1
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,03h	;AND SI,3
	MOV	ES,srcSeg
	MOV	CH,ES:[BX]
	AND	CH,mask4[SI]
	POP	SI
	MOV	BX,OFFSET DGROUP:dstLines
	ADD	BX,dstTop
	ADD	BX,DI
	MOV	BX,[BX]
	ADD	SI,dstLeft
	PUSH	SI
	SHR	SI,1
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,03h	;AND SI,3
	MOV	ES,dstSeg
	MOV	CL,nmask4[SI]
	AND	CL,ES:[BX]
	OR	CL,CH
	MOV	ES:[BX],CL
	DEC	BP
	JNZ	C_04AB
	CALL	waitTick
	MOV	BP,perWait
C_04AB:
	DEC	count
	JZ	C_04B4
	JMPN	C_0414
C_04B4:
	MOV	BX,OFFSET DGROUP:srcLines
	ADD	BX,srcTop
	MOV	BX,[BX]
	MOV	SI,srcLeft
	PUSH	SI
	SHR	SI,1
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,03h	;AND SI,3
	MOV	ES,srcSeg
	MOV	CH,ES:[BX]
	AND	CH,mask4[SI]
	MOV	BX,OFFSET DGROUP:dstLines
	ADD	BX,dstTop
	MOV	BX,[BX]
	MOV	SI,dstLeft
	PUSH	SI
	SHR	SI,1
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,03h	;AND SI,3
	MOV	ES,dstSeg
	MOV	CL,nmask4[SI]
	AND	CL,ES:[BX]
	OR	CL,CH
	MOV	ES:[BX],CL
ProcEnd	_DIS_dissolveCGA

;the same for 16 colors, 2 pixels a byte (Tandy)
ProcStart	_DIS_dissolveTDY	;13F2:0504
	MOV	AX,[BP+0Ah]
	MOV	perWait,AX
	CALL	FAR PTR DIS_setup
	MOV	AX,srcRight
	SUB	AX,srcLeft
	INC	AX
	MOV	winWidth,AX
	MOV	DX,srcBottom
	SUB	DX,srcTop
	INC	DX
	MOV	winHeight,DX
	SHL	srcTop,1
	SHL	dstTop,1
	MOV	lfsr,3
	MOV	lfsr+2,0
	MOV	xMask,1
	MOV	yShift,1
	MOV	CX,2
C_0550:
	SHR	AX,1
	JZ	C_0569
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	STC	
	RCL	xMask,1
	INC	yShift
	INC	CX
	JMP	SHORT C_0550
C_0569:
	SHR	DX,1
	JZ	C_0579
	STC	
	RCL	lfsr,1
	RCL	lfsr+2,1
	INC	CX
	JMP	SHORT C_0569
C_0579:
	MOV	DX,winHeight
	MOV	AX,winWidth
	MUL	DX
	DEC	AX
	MOV	count,AX
	MOV	BP,perWait
	MOV	BX,CX
	SHL	BX,1
	SHL	BX,1
	MOV	AX,tapTable[BX]
	MOV	taps,AX
	MOV	AX,tapTable+2[BX]
	MOV	taps+2,AX
C_059E:
	MOV	DX,lfsr+2
	MOV	AX,lfsr
	SHR	DX,1
	RCR	AX,1
	JNB	C_05B3
	XOR	DX,taps+2
	XOR	AX,taps
C_05B3:
	MOV	lfsr+2,DX
	MOV	lfsr,AX
	MOV	SI,AX
	AND	SI,xMask
	MOV	DI,AX
	MOV	CX,yShift
C_05C6:
	SHR	DX,1
	RCR	DI,1
	LOOP	C_05C6
	CMP	SI,winWidth
	JNB	C_059E
	CMP	DI,winHeight
	JNB	C_059E
	SHL	DI,1
	MOV	BX,OFFSET DGROUP:srcLines
	ADD	BX,srcTop
	ADD	BX,DI
	MOV	BX,[BX]
	PUSH	SI
	ADD	SI,srcLeft
	PUSH	SI
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,01h	;AND SI,1
	MOV	ES,srcSeg
	MOV	CL,ES:[BX]
	AND	CL,mask2[SI]
	MOV	BX,OFFSET DGROUP:dstLines
	ADD	BX,dstTop
	ADD	BX,DI
	MOV	BX,[BX]
	POP	SI
	ADD	SI,dstLeft
	PUSH	SI
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,01h	;AND SI,1
	MOV	ES,dstSeg
	MOV	CH,ES:[BX]
	AND	CH,nmask2[SI]
	OR	CL,CH
	MOV	ES:[BX],CL
	DEC	BP
	JNZ	C_0631
	CALL	waitTick
	MOV	BP,perWait
C_0631:
	DEC	count
	JZ	C_063A
	JMPN	C_059E
C_063A:
	MOV	BX,OFFSET DGROUP:srcLines
	ADD	BX,srcTop
	MOV	BX,[BX]
	MOV	SI,srcLeft
	PUSH	SI
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,01h	;AND SI,1
	MOV	ES,srcSeg
	MOV	CL,ES:[BX]
	AND	CL,mask2[SI]
	MOV	BX,OFFSET DGROUP:dstLines
	ADD	BX,dstTop
	MOV	BX,[BX]
	MOV	SI,dstLeft
	PUSH	SI
	SHR	SI,1
	ADD	BX,SI
	POP	SI
	db	83h,0E6h,01h	;AND SI,1
	MOV	ES,dstSeg
	MOV	CH,ES:[BX]
	AND	CH,nmask2[SI]
	OR	CL,CH
	MOV	ES:[BX],CL
ProcEnd	_DIS_dissolveTDY

CSegEnd	DISSOLVE_TEXT

END
