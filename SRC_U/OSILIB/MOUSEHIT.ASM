;----------------------------------------
;OSI library (U.EXE): mouse, is the pointer in a rectangle
;(GAME.EXE has these in its MOUSE.ASM)
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN _pMouse:WORD	;_pMouse
EXTRN _nMouseSem:WORD	;_nMouseSem
DSegEnd

CSegStart	MOUSEHIT_TEXT

;does the pointer shape (shown) overlap the window of a surface:
;_MOUSE_IN_AREA(struct tSurface *s)
ProcStart	_MOUSE_IN_AREA	;1344:0000
	PUSH	ES
	;--
	MOV	SI,_pMouse
	MOV	AH,0
	MOV	AL,[SI].installed
	CMP	AX,0
	JNZ	C_0018
	JMPN	C_00A1
C_0018:	;--
	MOV	ES,WORD PTR [SI].shapes+2
	MOV	DI,WORD PTR [SI].shapes
	MOV	BX,[SI].shapeIndex
	;-- ES:DI += offset
	MOV	AX,ES
	MOV	DX,0
	SHL	AX,1
	RCL	DX,1
	SHL	AX,1
	RCL	DX,1
	SHL	AX,1
	RCL	DX,1
	SHL	AX,1
	RCL	DX,1
	ADD	AX,DI
	ADC	DX,0
	SHL	BX,1
	SHL	BX,1
	ADD	BX,4
	ADD	AX,ES:[BX+DI]
	ADC	DX,ES:[BX+DI+02]
	MOV	DI,AX
	db	83h,0E7h,0Fh	;AND DI,0Fh
	SHR	DX,1
	RCR	AX,1
	SHR	DX,1
	RCR	AX,1
	SHR	DX,1
	RCR	AX,1
	SHR	DX,1
	RCR	AX,1
	MOV	ES,AX
	;--
	MOV	BX,[BP+06]
	MOV	AX,0
	MOV	CX,_nMouseSem
	CMP	CX,0
	JL	C_00A1
	;--
	MOV	CX,[SI].posX
	SUB	CX,ES:[DI+02]
	CMP	CX,[BX].right
	JG	C_00A1
	;--
	MOV	CX,[SI].posX
	ADD	CX,ES:[DI]
	CMP	CX,[BX].left
	JL	C_00A1
	;--
	MOV	CX,[SI].posY
	SUB	CX,ES:[DI+04]
	CMP	CX,[BX].bottom
	JG	C_00A1
	;--
	MOV	CX,[SI].posY
	ADD	CX,ES:[DI+06]
	CMP	CX,[BX].top
	JL	C_00A1
	;--
	MOV	AX,1
C_00A1:	;--
	POP	ES
ProcEnd	_MOUSE_IN_AREA

;is the pointer (its hot spot) in the window of a surface (and the mouse installed):
;_MOUSE_IN_RECT(struct tSurface *s)
ProcStart	_MOUSE_IN_RECT	;1344:00A7
	MOV	BX,[BP+06]
	;--
	MOV	SI,_pMouse
	MOV	AX,0
	CMP	BYTE PTR [SI].installed,0
	JZ	C_00DA
	;--
	MOV	DX,[SI].posX
	CMP	DX,[BX].right
	JG	C_00DA
	CMP	DX,[BX].left
	JL	C_00DA
	
	MOV	DX,[SI].posY
	CMP	DX,[BX].top
	JL	C_00DA
	CMP	DX,[BX].bottom
	JG	C_00DA
	;--
	MOV	AX,1
C_00DA:	;--
ProcEnd	_MOUSE_IN_RECT
;does a pointer shape put at (x,y) overlap the window of a surface:
;_SHAPE_IN_RECT(struct tSurface *s, void far *shapes, int index, int x, int y)
ProcStart	_SHAPE_IN_RECT	;1344:00DF
	CLD
	LES	DI,[BP+08]
	MOV	BX,[BP+0Ch]
	;-- ES:DI += offset
	MOV	AX,ES
	MOV	DX,0
	SHL	AX,1
	RCL	DX,1
	SHL	AX,1
	RCL	DX,1
	SHL	AX,1
	RCL	DX,1
	SHL	AX,1
	RCL	DX,1
	ADD	AX,DI
	ADC	DX,0
	SHL	BX,1
	SHL	BX,1
	ADD	BX,4
	ADD	AX,ES:[BX+DI]
	ADC	DX,ES:[BX+DI+02]
	MOV	DI,AX
	db	83h,0E7h,0Fh	;AND DI,0Fh
	SHR	DX,1
	RCR	AX,1
	SHR	DX,1
	RCR	AX,1
	SHR	DX,1
	RCR	AX,1
	SHR	DX,1
	RCR	AX,1
	MOV	ES,AX
	;--
	MOV	BX,[BP+06]
	MOV	AX,0
	MOV	CX,[BP+0Eh]
	SUB	CX,ES:[DI+02]
	CMP	CX,[BX+08]
	JL	C_0163
	;--
	MOV	CX,[BP+0Eh]
	ADD	CX,ES:[DI]
	CMP	CX,[BX+06]
	JG	C_0163
	;--
	MOV	CX,[BP+10h]
	SUB	CX,ES:[DI+04]
	CMP	CX,[BX+08+2]
	JL	C_0163
	;--
	MOV	CX,[BP+10h]
	ADD	CX,ES:[DI+06]
	CMP	CX,[BX+0Ch]
	JG	C_0163
	;--
	MOV	AX,1
C_0163:	;--
ProcEnd	_SHAPE_IN_RECT

CSegEnd	MOUSEHIT_TEXT

END
