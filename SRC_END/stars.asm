;
; Ultima VI - The False Prophet: END.EXE
; segment 02C1: the starfield of the ending, one routine per video mode
;
; Assembly (MASM 3.0, /Mx), not C: Origin's TASM prologue (push bp, mov bp,sp, push di,
; push si, push ds), registers Turbo C would not use this way, and near subroutines.
;
; 64 stars (starX, starY, starColor; 0..112 x 0..93, on the screen at x + 6Ah) fly out
; from the centre (56, 47): each tick a star is erased (if it still has its colour), moved
; away from the centre by 1 + distance/16 on each axis, and drawn again where the screen
; is black; one that leaves the field is reborn at a random place. Where it lands on a
; pixel of its own colour, its colour changes (EGA, CGA, Tandy).
;
; The routines are called by the timer callback (tick.c, C_02AE_0001), the EGA one with
; es = the page to draw on. Their exit pops seven words for the four pushed: they
; return over the callback's popf/retf, straight into the timer interrupt handler
; (TIMER.C), with ds = the bp pushed at their entry. The original does this; it is kept.
;
; The random numbers come from two 16-bit LFSRs (starSeedX, starSeedY, seeded by the
; main module) addressed through cs: (the linker makes their offsets relative to this
; segment, which reaches the same memory as ds:).
;

_DATA	SEGMENT PUBLIC WORD 'DATA'
EXTRN	_videoMode:WORD	;D_1816 video mode: 4 CGA, 7 Hercules, 9 Tandy, 0Dh EGA, 13h MCGA
EXTRN	_starSeedX:WORD	;D_1833 random generator of the stars' x
EXTRN	_starSeedY:WORD	;D_1835 random generator of the stars' y
EXTRN	_starColor:BYTE	;D_1E4C [64] the stars' colours
EXTRN	_starX:BYTE	;D_1EEC [64] the stars' x (0..112)
EXTRN	_starY:BYTE	;D_1F2C [64] the stars' y (0..93)
EXTRN	_scanlineOfs:WORD	;D_1F6C [200] the offset of each line on the screen
PUBLIC	egaBits,cgaMask,cgaClear,cgaShift,tdyMask,tdyClear,tdyShift
egaBits	db 80h,40h,20h,10h,8,4,2,1	;D_0E72 EGA: the bit of each pixel of a byte
cgaMask	db 0C0h,30h,0Ch,3	;D_0E7A CGA: the bits of each pixel of a byte
cgaClear	db 3Fh,0CFh,0F3h,0FCh	;D_0E7E CGA: the other pixels' bits
cgaShift	db 6,4,2,0	;D_0E82 CGA: the shift of each pixel
tdyMask	db 0F0h,0Fh	;D_0E86 Tandy: the bits of each pixel of a byte
tdyClear	db 0Fh,0F0h	;D_0E88 Tandy: the other pixel's bits
tdyShift	db 4,0	;D_0E8A Tandy: the shift of each pixel
_DATA	ENDS

DGROUP	GROUP	_DATA

STARS_TEXT	SEGMENT PUBLIC WORD 'CODE'
	ASSUME	CS:STARS_TEXT,DS:DGROUP

;-- move star si (dl = x, bl = y) away from the centre (56, 47) by 1 + distance/16 on each
;-- axis; leaves dl, bl = the new x, y
MOVE_STAR	MACRO	l1,l2,l3,l4,l5,l6
	SUB	DL,56
	PUSHF
	JNS	l1
	NEG	DL
l1:	SUB	BL,47
	PUSHF
	JNS	l2
	NEG	BL
l2:	MOV	CL,4
	SHR	DL,CL
	ADD	DL,1
	SHR	BL,CL
	ADD	BL,1
	POPF
	JS	l3
	ADD	_starY[SI],BL
	JMP	SHORT l4
l3:	SUB	_starY[SI],BL
l4:	MOV	BL,_starY[SI]
	POPF
	JS	l5
	ADD	_starX[SI],DL
	JMP	SHORT l6
l5:	SUB	_starX[SI],DL
l6:	MOV	DL,_starX[SI]
	ENDM

;-- a star that left the field: a random place, then draw it (at l)
NEW_STAR	MACRO	l
	CALL	RandX
	MOV	_starX[SI],AL
	CALL	RandY
	MOV	_starY[SI],AL
	MOV	BL,AL
	MOV	DL,_starX[SI]
	JMP	SHORT l
	ENDM

;-- the exit (see above: three words more than pushed)
STARS_EXIT	MACRO
	POP	DI
	POP	SI
	POP	BP
	POP	DS
	POP	SI
	POP	DI
	POP	BP
	RET
	ENDM

;C_02C1_000C: MCGA (320x200, a byte a pixel)
PUBLIC	_StarsMCGA
_StarsMCGA	PROC FAR
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
	MOV	AX,0A000h
	MOV	ES,AX
	MOV	SI,63
	MOV	BH,0
	MOV	DH,0
mcgaStar:
	MOV	DL,_starX[SI]
	MOV	BL,_starY[SI]
	MOV	CL,_starColor[SI]
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	SHR	BL,1
	ADD	DI,DX
	CMP	ES:[DI+6Ah],CL	;erase it if it is still there
	JNZ	mcgaMove
	MOV	BYTE PTR ES:[DI+6Ah],0
mcgaMove:
	MOVE_STAR	mcga1,mcga2,mcga3,mcga4,mcga5,mcga6
	CMP	BL,94
	JNB	mcgaNew
	CMP	DL,113
	JNB	mcgaNew
mcgaPlot:
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	SHR	BL,1
	ADD	DI,DX
	CMP	BYTE PTR ES:[DI+6Ah],0	;draw it on black only
	JNZ	mcgaNext
	MOV	CL,_starColor[SI]
	MOV	ES:[DI+6Ah],CL
mcgaNext:
	DEC	SI
	JS	mcgaEnd
	JMP	mcgaStar
_StarsMCGA	ENDP

;C_02C1_00A6: a random x, 0..112
RandX	PROC NEAR
	SHR	WORD PTR CS:_starSeedX,1
	JNB	randX1
	XOR	WORD PTR CS:_starSeedX,0B400h
randX1:	MOV	AX,CS:_starSeedX
	AND	AX,7Fh
	CMP	AX,112
	JA	randX2
	RET
randX2:	PUSH	DX
	MOV	DL,113
	DIV	DL
	MOV	AL,AH
	POP	DX
	RET
RandX	ENDP

;C_02C1_00CA: a random y, 0..93
RandY	PROC NEAR
	SHR	WORD PTR CS:_starSeedY,1
	JNB	randY1
	XOR	WORD PTR CS:_starSeedY,0B400h
randY1:	MOV	AX,CS:_starSeedY
	AND	AX,7Fh
	CMP	AX,93
	JA	randY2
	RET
randY2:	PUSH	DX
	MOV	DL,94
	DIV	DL
	MOV	AL,AH
	POP	DX
	RET
RandY	ENDP

;the end of _StarsMCGA
McgaTail	PROC FAR
mcgaNew:
	NEW_STAR	mcgaPlot
mcgaEnd:
	STARS_EXIT
McgaTail	ENDP

;C_02C1_010C: EGA (16 colours in 4 planes, a bit a pixel; es = the page)
PUBLIC	_StarsEGA
_StarsEGA	PROC FAR
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
	MOV	DX,3C4h
	MOV	AX,0F02h	;map mask: all planes
	OUT	DX,AX
	MOV	DX,3CEh
	MOV	AX,205h	;write mode 2
	OUT	DX,AX
	MOV	SI,63
egaStar:
	XOR	BH,BH
	XOR	DH,DH
	MOV	DL,_starX[SI]
	MOV	BL,_starY[SI]
	MOV	CL,_starColor[SI]
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	SHR	BL,1
	PUSH	DX
	ADD	DX,6Ah
	PUSH	DX
	SHR	DX,1
	SHR	DX,1
	SHR	DX,1
	ADD	DI,DX
	POP	BP
	db	83h,0E5h,07h	;AND BP,7 (MASM 3.0 has no short form for AND)
	MOV	CH,DS:egaBits[BP]
	MOV	DX,3CEh
	MOV	BP,BX
	XOR	BL,BL
	MOV	AX,304h	;read map 3..0: the pixel's colour in bl
egaRead:
	OUT	DX,AX
	MOV	BH,ES:[DI]
	AND	BH,CH
	NEG	BH
	ROL	BX,1
	DEC	AH
	JGE	egaRead
	MOV	AH,CH
	MOV	CH,BL
	MOV	BX,BP
	CMP	CH,CL	;erase it if it is still there
	JNZ	egaMove
	MOV	AL,8	;bit mask
	OUT	DX,AX
	XOR	CH,CH
	MOV	AH,ES:[DI]
	MOV	ES:[DI],CH
egaMove:
	POP	DX
	MOVE_STAR	ega1,ega2,ega3,ega4,ega5,ega6
	CMP	BL,94
	JNB	egaNew
	CMP	DL,113
	JNB	egaNew
egaPlot:
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	ADD	DX,6Ah
	PUSH	DX
	SHR	DX,1
	SHR	DX,1
	SHR	DX,1
	ADD	DI,DX
	POP	BP
	db	83h,0E5h,07h	;AND BP,7 (MASM 3.0 has no short form for AND)
	MOV	CH,DS:egaBits[BP]
	MOV	DX,3CEh
	XOR	BL,BL
	MOV	AX,304h
egaRead2:
	OUT	DX,AX
	MOV	BH,ES:[DI]
	AND	BH,CH
	NEG	BH
	ROL	BX,1
	DEC	AH
	JGE	egaRead2
	MOV	AH,CH
	OR	BL,BL	;draw it on black
	JNZ	egaLit
	MOV	AL,8
	OUT	DX,AX
	MOV	AL,_starColor[SI]
	MOV	AH,ES:[DI]
	STOSB
egaNext:
	DEC	SI
	JS	egaEnd
	JMP	egaStar
egaLit:	MOV	CH,_starColor[SI]	;on its own colour: the next colour (1..15)
	CMP	BL,CH
	JNZ	egaNext
	INC	CH
	AND	CH,0Fh
	JNZ	egaLit1
	INC	CH
egaLit1:	MOV	_starColor[SI],CH
	JMP	SHORT egaNext
egaNew:
	NEW_STAR	egaPlot
egaEnd:
	STARS_EXIT
_StarsEGA	ENDP

;C_02C1_0244: CGA (2 bits a pixel) and Hercules (the same, at B000h)
PUBLIC	_StarsCGA
_StarsCGA	PROC FAR
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
	MOV	AX,0B800h
	MOV	BX,CS:_videoMode
	CMP	BL,4
	JZ	cgaSeg
	MOV	AX,0B000h
cgaSeg:	MOV	ES,AX
	MOV	SI,63
	MOV	BH,0
	MOV	DH,0
cgaStar:
	MOV	DL,_starX[SI]
	MOV	BL,_starY[SI]
	MOV	CH,_starColor[SI]
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	SHR	BL,1
	PUSH	DX
	ADD	DX,6Ah
	MOV	BP,DX
	SHR	DX,1
	SHR	DX,1
	ADD	DI,DX
	db	83h,0E5h,03h	;AND BP,3 (MASM 3.0 has no short form for AND)
	MOV	AL,ES:[DI]
	AND	AL,DS:cgaMask[BP]
	MOV	CL,DS:cgaShift[BP]
	SHR	AL,CL
	POP	DX
	CMP	CH,AL	;erase it if it is still there
	JNZ	cgaMove
	MOV	AL,ES:[DI]
	AND	AL,DS:cgaClear[BP]
	MOV	ES:[DI],AL
cgaMove:
	MOVE_STAR	cga1,cga2,cga3,cga4,cga5,cga6
	CMP	BL,94
	JNB	cgaNew
	CMP	DL,113
	JNB	cgaNew
cgaPlot:
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	SHR	BL,1
	ADD	DX,6Ah
	MOV	BP,DX
	SHR	DX,1
	SHR	DX,1
	ADD	DI,DX
	db	83h,0E5h,03h	;AND BP,3 (MASM 3.0 has no short form for AND)
	MOV	AL,ES:[DI]
	AND	AL,DS:cgaMask[BP]
	MOV	CL,DS:cgaShift[BP]
	SHR	AL,CL
	OR	AL,AL	;draw it on black
	JNZ	cgaLit
	MOV	CH,_starColor[SI]
	MOV	CL,DS:cgaShift[BP]
	SHL	CH,CL
	MOV	AL,ES:[DI]
	AND	AL,DS:cgaClear[BP]
	OR	AL,CH
	STOSB
cgaNext:
	DEC	SI
	JS	cgaEnd
	JMP	cgaStar
cgaLit:	MOV	CH,_starColor[SI]	;on its own colour: the next colour (1..3)
	CMP	AL,CH
	JNZ	cgaNext
	INC	CH
	AND	CH,3
	JNZ	cgaLit1
	INC	CH
cgaLit1:	MOV	_starColor[SI],CH
	JMP	SHORT cgaNext
cgaNew:
	NEW_STAR	cgaPlot
cgaEnd:
	STARS_EXIT
_StarsCGA	ENDP

;C_02C1_0367: Tandy (16 colours, 4 bits a pixel)
PUBLIC	_StarsTandy
_StarsTandy	PROC FAR
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
	MOV	AX,0B800h
	MOV	ES,AX
	MOV	SI,63
	MOV	BH,0
	MOV	DH,0
tdyStar:
	MOV	DL,_starX[SI]
	MOV	BL,_starY[SI]
	MOV	CH,_starColor[SI]
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	SHR	BL,1
	PUSH	DX
	ADD	DX,6Ah
	MOV	BP,DX
	SHR	DX,1
	ADD	DI,DX
	db	83h,0E5h,01h	;AND BP,1 (MASM 3.0 has no short form for AND)
	MOV	AL,ES:[DI]
	AND	AL,DS:tdyMask[BP]
	MOV	CL,DS:tdyShift[BP]
	SHR	AL,CL
	POP	DX
	CMP	CH,AL	;erase it if it is still there
	JNZ	tdyMove
	MOV	AL,ES:[DI]
	AND	AL,DS:tdyClear[BP]
	MOV	ES:[DI],AL
tdyMove:
	MOVE_STAR	tdy1,tdy2,tdy3,tdy4,tdy5,tdy6
	CMP	BL,94
	JNB	tdyNew
	CMP	DL,113
	JNB	tdyNew
tdyPlot:
	SHL	BL,1
	MOV	DI,_scanlineOfs[BX]
	SHR	BL,1
	ADD	DX,6Ah
	MOV	BP,DX
	SHR	DX,1
	ADD	DI,DX
	db	83h,0E5h,01h	;AND BP,1 (MASM 3.0 has no short form for AND)
	MOV	AL,ES:[DI]
	AND	AL,DS:tdyMask[BP]
	MOV	CL,DS:tdyShift[BP]
	SHR	AL,CL
	OR	AL,AL	;draw it on black
	JNZ	tdyLit
	MOV	CH,_starColor[SI]
	MOV	CL,DS:tdyShift[BP]
	SHL	CH,CL
	MOV	AL,ES:[DI]
	AND	AL,DS:tdyClear[BP]
	OR	AL,CH
	STOSB
tdyNext:
	DEC	SI
	JS	tdyEnd
	JMP	tdyStar
tdyLit:	MOV	CH,_starColor[SI]	;on its own colour: the next colour (1..15)
	CMP	AL,CH
	JNZ	tdyNext
	INC	CH
	AND	CH,0Fh
	JNZ	tdyLit1
	INC	CH
tdyLit1:	MOV	_starColor[SI],CH
	JMP	SHORT tdyNext
tdyNew:
	NEW_STAR	tdyPlot
tdyEnd:
	STARS_EXIT
_StarsTandy	ENDP

STARS_TEXT	ENDS
	END
