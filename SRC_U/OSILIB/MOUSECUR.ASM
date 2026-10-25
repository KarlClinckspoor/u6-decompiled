;----------------------------------------
;OSI library (U.EXE): mouse, showing and hiding the pointer
;(GAME.EXE has these in its MOUSE.ASM, calling the graphics driver directly)
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN D_459E:WORD	;_pMouse
EXTRN D_45A0:WORD	;_nMouseSem

;the graphics driver's entries, set by the program
;(only their order is certain: they could belong to another module of the library)
PUBLIC drvDrawPointer,drvSavePointer,drvRestPointer,drvPutChar,drvFillRect
drvDrawPointer	dd 0	;D_45BC (struct tSurface *s, int x, int y, void far *shapes, int index)
drvSavePointer	dd 0	;D_45C0 (struct tSurface *s, void far *buf, int x, int y, void far *shapes, int index)
drvRestPointer	dd 0	;D_45C4 (the same)
drvPutChar	dd 0	;D_45C8 the character AX (BX: struct tText *, CX: height, DX: width, SI: 2*y)
drvFillRect	dd 0	;D_45CC (struct tSurface *rect, int color)
DSegEnd

EXTRN C_1368_0002:FAR	;wait for the vertical retrace

CSegStart	MOUSECUR_TEXT

;show the pointer (once as many times as it was hidden)
FProc	_MOUSE_SHOW	;1339:0000
	CMP	D_45A0,-1
	JNZ	C_0015
	INC	D_45A0
	CALL	FAR PTR _MOUSE_SAVE_BG
	CALL	FAR PTR _MOUSE_DRAW
C_0015:
	RET
_MOUSE_SHOW	ENDP

;hide the pointer
FProc	_MOUSE_HIDE	;1339:0016
	CMP	D_45A0,0
	JNZ	C_0037
	DEC	D_45A0
	XOR	AX,AX
	MOV	ES,AX
	MOV	AL,ES:[449h]	;0040:0049 the BIOS video mode
	CMP	AL,13h
	JNZ	C_0032
	CALL	C_1368_0002
C_0032:
	CALL	FAR PTR _MOUSE_RESTORE_BG
C_0037:
	RET
_MOUSE_HIDE	ENDP

FProc	_MOUSE_DRAW	;1339:0038
	MOV	BX,D_459E
	CMP	[BX].installed,0
	JZ	C_005B
	PUSH	[BX].shapeIndex
	PUSH	WORD PTR [BX].shapes+2
	PUSH	WORD PTR [BX].shapes
	PUSH	[BX].posY
	PUSH	[BX].posX
	PUSH	[BX].mSurface
	CALL	drvDrawPointer
	ADD	SP,0Ch
C_005B:
	RET
_MOUSE_DRAW	ENDP

FProc	_MOUSE_SAVE_BG	;1339:005C
	MOV	BX,D_459E
	CMP	[BX].installed,0
	JZ	C_0085
	PUSH	[BX].shapeIndex
	PUSH	WORD PTR [BX].shapes+2
	PUSH	WORD PTR [BX].shapes
	PUSH	[BX].posY
	PUSH	[BX].posX
	PUSH	WORD PTR [BX].saveBuf+2
	PUSH	WORD PTR [BX].saveBuf
	PUSH	[BX].mSurface
	CALL	drvSavePointer
	ADD	SP,10h
C_0085:
	RET
_MOUSE_SAVE_BG	ENDP

FProc	_MOUSE_RESTORE_BG	;1339:0086
	MOV	BX,D_459E
	CMP	[BX].installed,0
	JZ	C_00AF
	PUSH	[BX].shapeIndex
	PUSH	WORD PTR [BX].shapes+2
	PUSH	WORD PTR [BX].shapes
	PUSH	[BX].posY
	PUSH	[BX].posX
	PUSH	WORD PTR [BX].saveBuf+2
	PUSH	WORD PTR [BX].saveBuf
	PUSH	[BX].mSurface
	CALL	drvRestPointer
	ADD	SP,10h
C_00AF:
	RET
_MOUSE_RESTORE_BG	ENDP

CSegEnd	MOUSECUR_TEXT

END
