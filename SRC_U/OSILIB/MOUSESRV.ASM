;----------------------------------------
;OSI library (U.EXE): mouse, following its moves
;(GAME.EXE has these in its MOUSE.ASM)
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN D_459E:WORD	;_pMouse
EXTRN D_45A0:WORD	;_nMouseSem

lastX	dw 0	;D_45B4 the mouse state at the last update
lastY	dw 0	;D_45B6
lastL	dw 0	;D_45B8 left button (bit 0)
lastR	dw 0	;D_45BA right button (bit 1)
DSegEnd

EXTRN C_1339_0000:FAR	;_MOUSE_SHOW
EXTRN C_1339_0016:FAR	;_MOUSE_HIDE

CSegStart	MOUSESRV_TEXT

;update the mouse state, move the pointer and call onMove when it changed;
;returns 1 when it changed
ProcStart	_MOUSE_SERVE	;132C:0008
	MOV	BX,D_459E
	CMP	[BX].installed,0
	JZ	C_003D
	MOV	AX,3	;get mouse position and buttons status
	INT	33h
	SHR	CX,1	;horizontal pos / 2
	MOV	AX,lastL
	XOR	AL,BL
	AND	AL,1
	JNZ	C_0042
	MOV	AX,lastR
	XOR	AL,BL
	AND	AL,2
	JNZ	C_0042
	CMP	lastX,CX
	JNZ	C_0042
	CMP	lastY,DX
	JNZ	C_0042
C_003D:	;-- mouse state didn't change
	MOV	AX,0
	JMP	SHORT C_0095
C_0042:
	MOV	AX,D_45A0	;(the pointer is moved only when it is shown)
	PUSH	AX
	PUSH	BX
	PUSH	CX
	PUSH	DX
	OR	AX,AX
	JNZ	C_0052
	CALL	C_1339_0016
C_0052:
	POP	DX
	POP	CX
	POP	BX
	MOV	lastX,CX
	MOV	lastY,DX
	MOV	lastL,BX
	db	83h,26h	;AND lastL,1
	dw	OFFSET DGROUP:lastL
	db	01h
	MOV	lastR,BX
	db	83h,26h	;AND lastR,2
	dw	OFFSET DGROUP:lastR
	db	02h
	MOV	SI,D_459E
	MOV	[SI].posX,CX
	MOV	[SI].posY,DX
	MOV	[SI].buttonL,BL
	AND	[SI].buttonL,1
	MOV	[SI].buttonR,BL
	AND	[SI].buttonR,2
	CALL	[SI].onMove
	POP	AX
	OR	AX,AX
	JNZ	C_0092
	CALL	C_1339_0000
C_0092:
	MOV	AX,1
C_0095:
ProcEnd	_MOUSE_SERVE

;force the next update
ProcStart	_MOUSE_RESET	;132C:009A
	MOV	lastX,8001h
ProcEnd	_MOUSE_RESET

CSegEnd	MOUSESRV_TEXT

END
