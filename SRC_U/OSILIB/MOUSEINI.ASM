;----------------------------------------
;OSI library (U.EXE): mouse, initialisation and pointer shape
;(GAME.EXE has these in its MOUSE.ASM)
;----------------------------------------
INCLUDE OSI.INC

DSegStart
PUBLIC _pMouse,_nMouseSem
_pMouse	dw 0	;D_459E struct tMouse *
_nMouseSem	dw 0	;D_45A0 the pointer is shown when 0, hidden when below
__45A2	db 0,0	;?? (GAME.EXE's MOUSE.ASM has the same 2 bytes after _nMouseSem)
DSegEnd

EXTRN _MOUSE_SERVE:FAR	;_MOUSE_SERVE
EXTRN _MOUSE_RESET:FAR	;_MOUSE_RESET: force the next update
EXTRN _MOUSE_SHOW:FAR	;_MOUSE_SHOW
EXTRN _MOUSE_HIDE:FAR	;_MOUSE_HIDE

CSegStart	MOUSEINI_TEXT

;initialise the mouse: _MOUSE_INIT(struct tMouse *m), returns the number of buttons
;(nothing when there is no mouse)
ProcStart	_MOUSE_INIT	;12DD:000A
	MOV	BX,[BP+06]
	MOV	_pMouse,BX
	MOV	[BX].buttonL,0
	MOV	[BX].buttonR,0
	MOV	[BX].posX,0
	MOV	[BX].posY,0
	MOV	WORD PTR [BX].onMove,OFFSET MOUSEINI_TEXT:_MOUSE_NOP
	MOV	WORD PTR [BX].onMove+2,SEG _MOUSE_NOP
	MOV	[BX].installed,0
	MOV	AX,0	;mouse reset/get mouse installed flag
	INT	33h
	CMP	AX,0
	JZ	C_005A
	PUSH	BX	;the number of buttons
	MOV	BX,_pMouse
	MOV	[BX].installed,1
	MOV	_nMouseSem,-1
	CALL	_MOUSE_RESET
	CALL	_MOUSE_SERVE
	POP	AX
C_005A:
ProcEnd	_MOUSE_INIT

;the default onMove callback
PUBLIC _MOUSE_NOP
_MOUSE_NOP	PROC FAR	;12DD:005F
	RET
_MOUSE_NOP	ENDP

;change the pointer shape: _MOUSE_POINTER(void far *shapes, int index)
ProcStart	_MOUSE_POINTER	;12DD:0060
	LES	DI,[BP+06]
	MOV	AX,[BP+0Ah]
	MOV	BX,_pMouse
	CMP	[BX].installed,0
	JZ	C_00A4
	CMP	AX,[BX].shapeIndex
	JNZ	C_0087
	MOV	AX,ES
	CMP	AX,WORD PTR [BX].shapes+2
	JNZ	C_0087
	CMP	DI,WORD PTR [BX].shapes
	JZ	C_00A4
C_0087:
	CALL	_MOUSE_HIDE
	LES	DI,[BP+06]
	MOV	AX,[BP+0Ah]
	MOV	BX,_pMouse
	MOV	[BX].shapeIndex,AX
	MOV	WORD PTR [BX].shapes+2,ES
	MOV	WORD PTR [BX].shapes,DI
	CALL	_MOUSE_SHOW
C_00A4:
ProcEnd	_MOUSE_POINTER

;_MOUSE_INIT without the mouse driver: _MOUSE_NOINIT(struct tMouse *m)
ProcStart	_MOUSE_NOINIT	;12DD:00A9
	MOV	BX,[BP+06]
	MOV	_pMouse,BX
	MOV	[BX].buttonL,0
	MOV	[BX].buttonR,0
	MOV	[BX].posX,0
	MOV	[BX].posY,0
	MOV	WORD PTR [BX].onMove,OFFSET MOUSEINI_TEXT:_MOUSE_NOP
	MOV	WORD PTR [BX].onMove+2,SEG _MOUSE_NOP
	MOV	[BX].installed,0
ProcEnd	_MOUSE_NOINIT

CSegEnd	MOUSEINI_TEXT

END
