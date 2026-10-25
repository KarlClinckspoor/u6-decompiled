;----------------------------------------
;OSI library (U.EXE): the VGA palette (DAC)
;----------------------------------------
INCLUDE OSI.INC

DSegStart
palHandle	dw 0	;D_49FA
DSegEnd

EXTRN C_1368_0015:FAR	;wait for the vertical retrace

CSegStart	PALETTE_TEXT

;load the 256 colors of a palette file (at 30h, 3 bytes a color, 8 bits each) into the
;DAC: PAL_load(char *name), returns the high byte of the file's word at 20h (-1 on error)
ProcStart	PAL_load	;1475:0002
	MOV	AH,48h	;allocate memory
	MOV	BX,31h
	INT	21h
	MOV	ES,AX
	JNB	C_0016
	JMP	C_0091	;(SHORT forgotten: MASM adds a NOP)
C_0016:
	MOV	AX,3D00h	;open file
	MOV	DX,[BP+06]
	INT	21h
	JB	C_008D
	MOV	palHandle,AX
	MOV	AH,42h	;move file pointer
	MOV	AL,0
	MOV	CX,0
	MOV	DX,20h
	MOV	BX,palHandle
	INT	21h
	MOV	BX,palHandle
	PUSH	DS
	PUSH	ES
	POP	DS
	MOV	DX,0
	MOV	AH,3Fh	;read file
	MOV	CX,310h
	INT	21h
	MOV	CL,2	;8 bits to the DAC's 6
	MOV	DI,2FFh
C_0049:
	SHR	BYTE PTR [DI+10h],CL
	DEC	DI
	JNS	C_0049
	MOV	BX,0
	MOV	DX,10h
C_0055:	;-- 16 colors each vertical retrace
	PUSH	BX
	PUSH	DX
	PUSH	ES
	CALL	C_1368_0015
	POP	ES
	POP	DX
	POP	BX
	MOV	CX,10h
	MOV	AX,1012h	;set block of DAC registers
	INT	10h
	ADD	BX,10h
	ADD	DX,30h
	CMP	BX,100h
	JNZ	C_0055
	POP	DS
	PUSH	WORD PTR ES:[0]
	MOV	AH,3Eh	;close file
	MOV	BX,palHandle
	INT	21h
	MOV	AH,49h	;free memory
	INT	21h
	POP	AX
	MOV	AL,AH
	MOV	AH,0
	JMP	SHORT C_0094
C_008D:
	MOV	AH,49h	;free memory
	INT	21h
C_0091:
	MOV	AX,-1
C_0094:
ProcEnd	PAL_load

;set a color: PAL_set(int index, int *rgb)
ProcStart	PAL_set	;1475:0099
	MOV	DX,3C8h
	PUSHF
	CLI
	MOV	AX,[BP+06]
	OUT	DX,AL
	MOV	SI,[BP+08]
	INC	DX
	LODSW
	OUT	DX,AL
	LODSW
	OUT	DX,AL
	LODSW
	OUT	DX,AL
	POPF
ProcEnd	PAL_set

;get a color: PAL_get(int index, int *rgb)
ProcStart	PAL_get	;1475:00B8
	PUSH	DS
	POP	ES
	CLD
	PUSHF
	CLI
	MOV	DX,3C7h
	MOV	AX,[BP+06]
	OUT	DX,AL
	MOV	DI,[BP+08]
	MOV	DX,3C9h
	MOV	AH,0
	IN	AL,DX
	STOSW
	IN	AL,DX
	STOSW
	IN	AL,DX
	STOSW
	POPF
ProcEnd	PAL_get

CSegEnd	PALETTE_TEXT

END
