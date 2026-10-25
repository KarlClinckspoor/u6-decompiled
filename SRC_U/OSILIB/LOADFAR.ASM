;----------------------------------------
;OSI library (U.EXE): loading a whole file into far memory
;----------------------------------------
INCLUDE OSI.INC

DSegStart
farHandle	dw 0	;D_49F0
farBufSeg	dw 0	;D_49F2 the buffer (segment, offset)
farBufOff	dw 0	;D_49F4
farSize	dw 0,0	;D_49F6
DSegEnd

EXTRN _farmalloc:FAR,_farfree:FAR

CSegStart	LOADFAR_TEXT

;load a whole file into memory it farmallocs: LOAD_far(char *name), returns the
;buffer (NULL on error)
ProcStart	LOAD_far	;1463:0002
	MOV	AX,3D00h	;open file (read)
	MOV	DX,[BP+06]
	INT	21h
	JNB	C_0015
	JMPN	C_00A4
C_0015:
	MOV	farHandle,AX
	MOV	BX,AX
	MOV	AX,4202h	;move file pointer (to the end: the size)
	MOV	CX,0
	MOV	DX,0
	INT	21h
	JB	C_009C
	MOV	farSize+2,DX
	MOV	farSize,AX
	MOV	AX,4200h	;move file pointer (back to the start)
	MOV	BX,farHandle
	MOV	CX,0
	MOV	DX,0
	INT	21h
	JB	C_009C
	PUSH	farSize+2
	PUSH	farSize
	CALL	_farmalloc
	ADD	SP,4
	MOV	BX,AX
	OR	AX,DX
	JZ	C_009C
	MOV	farBufOff,BX
	MOV	farBufSeg,DX
	PUSH	DS
	MOV	CX,farSize
	MOV	BX,farHandle
	MOV	DX,farBufOff
	MOV	DS,farBufSeg
	MOV	AH,3Fh	;read file
	INT	21h
	POP	DS
	JB	C_008C
	CMP	AX,farSize
	JNZ	C_008C
	MOV	AH,3Eh	;close file
	MOV	BX,farHandle
	INT	21h
	MOV	AX,farBufOff
	MOV	DX,farBufSeg
	JMP	SHORT C_00A9
C_008C:
	PUSH	farBufSeg
	PUSH	farBufOff
	CALL	_farfree
	ADD	SP,4
C_009C:
	MOV	AH,3Eh	;close file
	MOV	BX,farHandle
	INT	21h
C_00A4:
	MOV	AX,0
	MOV	DX,AX
C_00A9:
ProcEnd	LOAD_far

;load a whole file (smaller than 64K) into a buffer: LOAD_to(char *name, void far *buf),
;returns its size (0 on error)
ProcStart	LOAD_to	;1463:00AE
	MOV	AX,3D00h	;open file (read)
	MOV	DX,[BP+06]
	INT	21h
	JB	C_0119
	MOV	farHandle,AX
	MOV	BX,AX
	MOV	AX,4202h	;move file pointer (to the end: the size)
	MOV	CX,0
	MOV	DX,0
	INT	21h
	JB	C_0111
	MOV	farSize+2,DX
	MOV	farSize,AX
	MOV	AX,4200h	;move file pointer (back to the start)
	MOV	BX,farHandle
	MOV	CX,0
	MOV	DX,0
	INT	21h
	JB	C_0111
	PUSH	DS
	MOV	CX,farSize
	MOV	BX,farHandle
	MOV	DX,[BP+08]
	MOV	DS,[BP+0Ah]
	MOV	AH,3Fh	;read file
	INT	21h
	POP	DS
	JB	C_0111
	CMP	AX,farSize
	JNZ	C_0111
	MOV	AH,3Eh	;close file
	MOV	BX,farHandle
	INT	21h
	MOV	AX,farSize
	JMP	SHORT C_011C
C_0111:
	MOV	AH,3Eh	;close file
	MOV	BX,farHandle
	INT	21h
C_0119:
	MOV	AX,0
C_011C:
ProcEnd	LOAD_to

CSegEnd	LOADFAR_TEXT

END
