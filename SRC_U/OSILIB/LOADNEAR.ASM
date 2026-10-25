;----------------------------------------
;OSI library (U.EXE): loading a whole file into near memory
;----------------------------------------
INCLUDE OSI.INC

DSegStart
nearHandle	dw 0	;D_49EA
nearBuf	dw 0	;D_49EC
nearSize	dw 0	;D_49EE
DSegEnd

EXTRN _malloc:FAR,_free:FAR

CSegStart	LOADNEAR_TEXT

;load a whole file (smaller than 64K) into memory it mallocs: LOAD_near(char *name),
;returns the buffer (NULL on error)
ProcStart	LOAD_near	;145A:0006
	MOV	AX,3D00h
	MOV	DX,[BP+06]
	INT	21h
	JB	C_008A
	MOV	nearHandle,AX
	MOV	BX,AX
	MOV	AX,4202h
	MOV	CX,0
	MOV	DX,0
	INT	21h
	JB	C_0082
	OR	DX,DX
	JNZ	C_0082
	MOV	nearSize,AX
	MOV	AX,4200h
	MOV	BX,nearHandle
	MOV	CX,0
	MOV	DX,0
	INT	21h
	JB	C_0082
	PUSH	nearSize
	CALL	_malloc
	ADD	SP,2
	OR	AX,AX
	JZ	C_0082
	MOV	nearBuf,AX
	MOV	DX,AX
	MOV	CX,nearSize
	MOV	BX,nearHandle
	MOV	AH,3Fh
	INT	21h
	JB	C_0076
	CMP	AX,nearSize
	JNZ	C_0076
	MOV	AH,3Eh
	MOV	BX,nearHandle
	INT	21h
	MOV	AX,nearBuf
	JMP	SHORT C_008D
C_0076:
	PUSH	nearBuf
	CALL	_free
	ADD	SP,2
C_0082:
	MOV	AH,3Eh
	MOV	BX,nearHandle
	INT	21h
C_008A:
	MOV	AX,0
C_008D:
ProcEnd	LOAD_near

CSegEnd	LOADNEAR_TEXT

END
