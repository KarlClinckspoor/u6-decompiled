;----------------------------------------
;OSI library (U.EXE): loading an LZW compressed file (as GAME.EXE's INFLATE.ASM)
;----------------------------------------
;TASM fixup order: 0046 000D 002A   (TASM wrote these fixups in this order: tools/u/tasmorder.py)
INCLUDE OSI.INC

DSegStart

masks	dw 1FFh,3FFh,7FFh,0FFFh	;D_45E4
input_handle	dw 0	;D_45EC
hash_seg	dw 0	;D_45EE
output_offset	dw 0	;D_45F0
bit_offset	dw 0	;D_45F2
cur_code	dw 0	;D_45F4
old_code	dw 0	;D_45F6
in_code	dw 0	;D_45F8
free_code	dw 0	;D_45FA
stack_count	dw 0	;D_45FC
max_code	dw 0	;D_45FE
nbits	dw 0	;D_4600
fin_char	db 0	;D_4602
k	db 0	;D_4603
segment_0	dw 0	;D_4604 the dictionary, a 3*1000h segment
lDestSz	dd 0	;D_4606 the size of the data (the file's header)
output_segment	dw 0	;D_460A
lpDest	dw 0,0	;D_460C segment, offset of the data
input_segment	dw 0	;D_4610 the input buffer, a 1000h segment

DSegEnd

EXTRN _malloc:FAR,_free:FAR,_farmalloc:FAR

CSegStart	LZW_TEXT

;load an LZW compressed file into memory it allocates: LZW_load(char *name),
;returns the data (NULL on error)
ProcStart	_LZW_load	;13BC:000C
	MOV	AX,3010h
	PUSH	AX
	CALL	_malloc
	ADD	SP,2
	PUSH	AX	;(the argument of the free below)
	MOV	DX,DS
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	ADD	AX,DX
	INC	AX
	MOV	segment_0,AX
	MOV	AX,1010h
	PUSH	AX
	CALL	_malloc
	ADD	SP,2
	PUSH	AX
	MOV	DX,DS
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	ADD	AX,DX
	INC	AX
	MOV	input_segment,AX
	MOV	DX,[BP+06]
	CALL	FAR PTR LZW_loadFile
	JB	C_0072
	CALL	_free
	ADD	SP,2
	CALL	_free
	ADD	SP,2
	MOV	DX,lpDest
	MOV	AX,lpDest+2
	ProcRet
C_0072:
	CALL	_free
	ADD	SP,2
	CALL	_free
	ADD	SP,2
	MOV	DX,0
	MOV	AX,0
ProcEnd	_LZW_load

INCLUDE LZ.INC

;allocate BX paragraphs: DX:AX
alloc	PROC FAR	;13BC:02B5
	XOR	DX,DX
	SHL	BX,1
	RCL	DX,1
	SHL	BX,1
	RCL	DX,1
	SHL	BX,1
	RCL	DX,1
	SHL	BX,1
	RCL	DX,1
	PUSH	DX
	PUSH	BX
	CALL	_farmalloc
	ADD	SP,4
	CLC
	RET
alloc	ENDP

;open the file DS:DX, allocate and decompress; CF set on error
LZW_loadFile	PROC FAR	;13BC:02D3
	MOV	output_segment,0
	MOV	output_offset,0
	MOV	lpDest,0
	MOV	input_handle,0
	MOV	AL,2
	MOV	AH,3Dh	;open file
	INT	21h
	MOV	BX,AX
	JB	C_0341
	MOV	input_handle,BX
	;-- read Z-file header
	MOV	CX,4
	MOV	DX,OFFSET DGROUP:lDestSz
	MOV	AH,3Fh	;read file
	INT	21h
	JB	C_0341
	MOV	BX,WORD PTR lDestSz
	MOV	DX,WORD PTR lDestSz+2
	MOV	CL,4
C_030F:
	SHR	DX,1
	RCR	BX,1
	LOOP	C_030F
	INC	BX
	CALL	alloc
	JNB	C_0325
	MOV	output_segment,-1
	JMP	SHORT C_0341
C_0325:
	MOV	output_offset,AX
	MOV	lpDest+2,AX
	MOV	output_segment,DX
	MOV	lpDest,DX
	CALL	decompress
	CLC
	MOV	AX,WORD PTR lDestSz
	OR	AX,AX
	JNZ	C_0341
	DEC	AX
C_0341:
	PUSHF
	PUSH	AX
	MOV	BX,input_handle
	JZ	C_034D
	MOV	AH,3Eh	;close file
	INT	21h
C_034D:
	MOV	BX,output_segment
	CMP	BX,-1
	JNZ	C_035B
	INC	BX
	POP	AX
	POPF
C_0359:	JMP	C_0359	;out of memory: hang
C_035B:
	POP	AX
	POPF
	JNB	C_0364
	MOV	BX,1
	STC
	RET
C_0364:
	MOV	BX,lpDest
	RET
LZW_loadFile	ENDP

CSegEnd	LZW_TEXT

END
