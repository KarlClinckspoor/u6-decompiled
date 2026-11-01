;
; Ultima VI - The False Prophet: U.EXE
; segment 0F18: loading an LZW compressed (".z" style) file into memory
;
; Assembly (MASM 3.0, /Mx), not C. The decompressor is the one of GAME.EXE's
; SRC/OSILIB/INFLATE.ASM and LZ.INC (very close to Tom Pfau's code); here it is given a
; file name, opens the file itself and allocates the memory for the result.
; The file: a 4-byte length of the decompressed data, then the LZW codes (9 to 12 bits).
;
;TASM fixup order: 0046 000D 002A   (TASM wrote these fixups in this order: tools/u/tasmorder.py)

CLEAR      EQU 256	;code: reset the table
EOF        EQU 257	;code: end of the data
FIRST_FREE EQU 258

_DATA	SEGMENT PUBLIC WORD 'DATA'
PUBLIC	lzwMasks,lzwInput,lzwHashSeg,lzwOutOfs,lzwBitOfs,lzwCurCode,lzwOldCode,lzwInCode,lzwFreeCode,lzwStackCount
PUBLIC	lzwMaxCode,lzwNBits,lzwFinChar,lzwK,lzwTableSeg,lzwDestSize,lzwOutSeg,lzwDest,lzwInSeg
lzwMasks	dw 1FFh,3FFh,7FFh,0FFFh	; D_4110 masks: a code of 9..12 bits
lzwInput	dw 0	; D_4118 input_handle
lzwHashSeg	dw 0	; D_411A hash_seg: the string table (3 bytes a code)
lzwOutOfs	dw 0	; D_411C output_offset
lzwBitOfs	dw 0	; D_411E bit_offset: in the input buffer
lzwCurCode	dw 0	; D_4120 cur_code
lzwOldCode	dw 0	; D_4122 old_code
lzwInCode	dw 0	; D_4124 in_code
lzwFreeCode	dw 0	; D_4126 free_code
lzwStackCount	dw 0	; D_4128 stack_count
lzwMaxCode	dw 0	; D_412A max_code
lzwNBits	dw 0	; D_412C nbits: the size of a code
lzwFinChar	db 0	; D_412E fin_char
lzwK	db 0	; D_412F k
lzwTableSeg	dw 0	; D_4130 segment_0: a 3010h bytes buffer (the string table)
lzwDestSize	dd 0	; D_4132 lDestSz: the decompressed size, from the file header
lzwOutSeg	dw 0	; D_4136 output_segment
lzwDest	dw 0,0	; D_4138 lpDest: segment, offset of the result
lzwInSeg	dw 0	; D_413C input_segment: a 1010h bytes buffer (1 KB of input)
_DATA	ENDS

DGROUP	GROUP	_DATA

	EXTRN	_malloc:FAR
	EXTRN	_free:FAR
	EXTRN	_farmalloc:FAR

LOADZ_TEXT	SEGMENT PUBLIC WORD 'CODE'
	ASSUME	CS:LOADZ_TEXT,DS:DGROUP

;-- the next code from the input buffer, in ax (refills the buffer near its end)
read_code	MACRO
	LOCAL rd0,rd1,rd2
	PUSH	BP

	MOV	AX,lzwBitOfs	;bit offset
	ADD	AX,lzwNBits	;adjusted by the code size
	XCHG	AX,lzwBitOfs
	MOV	CX,8
	XOR	DX,DX
	DIV	CX
	CMP	AX,1021	;approaching the end of the buffer?
	JL	rd0	;no

	PUSH	DX	;offset in the byte
	ADD	DX,lzwNBits	;new bit offset
	MOV	lzwBitOfs,DX
	MOV	CX,1024	;1 KB buffer
	MOV	BP,AX	;byte offset
	SUB	CX,AX	;bytes left
	;-- move them to the start and read the rest
	PUSH	DS
	MOV	BX,lzwInput
	MOV	DS,lzwInSeg
	PUSH	DS
	POP	ES
	XOR	SI,SI
	MOV	DI,SI
	ADD	SI,AX
	REPZ	MOVSB
	MOV	CX,BP
	MOV	DX,DI
	MOV	AH,3Fh	;read file
	INT	21h
	POP	DS
	;-- --
	XOR	AX,AX
	POP	DX
rd0:
	PUSH	DS
	MOV	DS,lzwInSeg
	MOV	SI,AX
	LODSW	;a word
	MOV	BX,AX
	LODSB	;and the next byte
	MOV	CX,DX	;offset in the byte
	JCXZ	rd2
rd1:	SHR	AL,1	;the code in the low (code size) bits of bx
	RCR	BX,1
	LOOP	rd1
rd2:
	POP	DS
	MOV	AX,BX
	MOV	BX,lzwNBits	;mask off the unwanted bits
	SUB	BX,9
	SHL	BX,1
	AND	AX,lzwMasks[BX]

	POP	BP
ENDM

init_tab	MACRO
	MOV	lzwNBits,9
	MOV	lzwMaxCode,512
	MOV	lzwFreeCode,FIRST_FREE
ENDM

;-- one byte (al) of output, normalising the far pointer every 16 bytes
write_char	MACRO
	LOCAL wc1

	PUSH	ES
	MOV	ES,lzwOutSeg
	MOV	DI,lzwOutOfs
	STOSB
	POP	ES
	INC	lzwOutOfs
	CMP	DI,10h
	JNZ	wc1
	INC	lzwOutSeg
	MOV	lzwOutOfs,0
wc1:
ENDM

;-- bx = bx * 3 (3-byte entries)
index	MACRO
	PUSH	AX
	MOV	AX,BX
	SHL	BX,1
	ADD	BX,AX
	POP	AX
ENDM

;-- the table entry free_code = old_code + k
add_code	MACRO
	MOV	BX,lzwFreeCode
	index
	PUSH	ES
	MOV	ES,lzwHashSeg
	MOV	AL,lzwK	;suffix char
	MOV	ES:2[BX],AL
	MOV	AX,lzwOldCode	;prefix code
	MOV	ES:[BX],AX
	POP	ES
	INC	lzwFreeCode
ENDM

;C_0F18_000E: void far *LoadZFile(char *name): the decompressed file in newly
;allocated memory, 0 if it cannot be read
PUBLIC	_LoadZFile
_LoadZFile	PROC FAR	;0F18:000E
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
	MOV	AX,3010h	;the string table
	PUSH	AX
	CALL	_malloc
	ADD	SP,2
	PUSH	AX	;(for free)
	MOV	DX,DS	;its first paragraph
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	ADD	AX,DX
	INC	AX
	MOV	lzwTableSeg,AX
	MOV	AX,1010h	;the input buffer
	PUSH	AX
	CALL	_malloc
	ADD	SP,2
	PUSH	AX	;(for free)
	MOV	DX,DS
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	SHR	AX,1
	ADD	AX,DX
	INC	AX
	MOV	lzwInSeg,AX
	MOV	DX,[BP+6]	;file name
	CALL	FAR PTR LoadZ
	JC	lz_fail
	CALL	_free
	ADD	SP,2
	CALL	_free
	ADD	SP,2
	MOV	DX,lzwDest
	MOV	AX,lzwDest+2
	POP	DS
	POP	SI
	POP	DI
	POP	BP
	RET
lz_fail:
	CALL	_free
	ADD	SP,2
	CALL	_free
	ADD	SP,2
	MOV	DX,0
	MOV	AX,0
	POP	DS
	POP	SI
	POP	DI
	POP	BP
	RET
_LoadZFile	ENDP

;C_0F18_008F: decompresses the open file input_handle to output_segment:output_offset
decompress	PROC FAR
	MOV	lzwNBits,9
	MOV	lzwBitOfs,0
	MOV	lzwFreeCode,FIRST_FREE
	MOV	lzwMaxCode,512

	MOV	AX,lzwTableSeg
	MOV	lzwHashSeg,AX

	MOV	BX,lzwInput
	PUSH	DS
	MOV	DS,lzwInSeg
	XOR	DX,DX
	MOV	CX,1024
	MOV	AH,3Fh	;read file
	INT	21h
	POP	DS

L1:	read_code
	CMP	AX,EOF
	JNE	L2

	MOV	CX,1800h	;clear the string table
	XOR	AX,AX
	MOV	ES,lzwHashSeg
	MOV	DI,AX
	REPZ	STOSW

	RET	;done

L2:	CMP	AX,CLEAR
	JE	L3
	JMP	L7
L3:
	init_tab
	read_code
	MOV	lzwCurCode,AX
	MOV	lzwOldCode,AX
	MOV	lzwK,AL
	MOV	lzwFinChar,AL
	write_char
	JMP	L1

L7:	MOV	lzwCurCode,AX	;a new code
	MOV	lzwInCode,AX
	MOV	ES,lzwHashSeg
	CMP	AX,lzwFreeCode	;in the table? (k<w>k<w>k)
	JL	L11	;yes
	MOV	AX,lzwOldCode	;the previous code
	MOV	lzwCurCode,AX
	MOV	AL,lzwFinChar	;its last char
	PUSH	AX
	INC	lzwStackCount
L11:	CMP	lzwCurCode,255	;code or char?
	JLE	L15	;char

	MOV	BX,lzwCurCode
	index
	MOV	AL,ES:2[BX]	;suffix char
	PUSH	AX
	INC	lzwStackCount
	MOV	AX,ES:[BX]	;prefix code
	MOV	lzwCurCode,AX
	JMP	L11

L15:
	PUSH	DS
	POP	ES
	MOV	AX,lzwCurCode
	MOV	lzwFinChar,AL
	MOV	lzwK,AL
	PUSH	AX
	INC	lzwStackCount
	MOV	CX,lzwStackCount	;output the stacked chars
	JCXZ	L18
L17:	POP	AX
	write_char
	LOOP	L17
L18:	MOV	lzwStackCount,CX
	add_code
	MOV	AX,lzwInCode
	MOV	lzwOldCode,AX
	MOV	BX,lzwFreeCode	;table full?
	CMP	BX,lzwMaxCode
	JL	L23
	CMP	lzwNBits,12	;already twelve bits?
	JE	L23
	INC	lzwNBits	;longer codes
	SHL	lzwMaxCode,1
L23:	JMP	L1
decompress	ENDP

;C_0F18_02B7: farmalloc((bx + 1) paragraphs), returned as dx:0 (the next paragraph
;boundary); the carry is always clear
alloc_para	PROC FAR
	INC	BX
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
	OR	AX,AX
	JZ	ap1
	XOR	AX,AX
	INC	DX
ap1:	CLC
	RET
alloc_para	ENDP

;C_0F18_02DD: opens the file named at ds:dx, allocates the memory and decompresses
;it; the carry set on error
LoadZ	PROC FAR
	MOV	lzwOutSeg,0
	MOV	lzwOutOfs,0
	MOV	lzwDest,0
	MOV	lzwInput,0
	MOV	AL,2
	MOV	AH,3Dh	;open file
	INT	21h
	MOV	BX,AX
	JC	lz1
	MOV	lzwInput,BX
	;-- the header: the decompressed size
	MOV	CX,4
	MOV	DX,OFFSET DGROUP:lzwDestSize
	MOV	AH,3Fh	;read file
	INT	21h
	JC	lz1
	MOV	BX,WORD PTR lzwDestSize
	MOV	DX,WORD PTR lzwDestSize+2
	MOV	CL,4
lz0:	SHR	DX,1
	RCR	BX,1
	LOOP	lz0
	INC	BX
	CALL	alloc_para
	JNC	lz2
	MOV	lzwOutSeg,-1
	JMP	SHORT lz1
lz2:	MOV	lzwOutOfs,AX
	MOV	lzwDest+2,AX
	MOV	lzwOutSeg,DX
	MOV	lzwDest,DX
	CALL	decompress
	CLC
	MOV	AX,WORD PTR lzwDestSize
	OR	AX,AX
	JNZ	lz1
	DEC	AX
lz1:	PUSHF
	PUSH	AX
	MOV	BX,lzwInput
	JZ	lz3
	MOV	AH,3Eh	;close file
	INT	21h
lz3:	MOV	BX,lzwOutSeg
	CMP	BX,-1
	JNZ	lz5
	INC	BX
	POP	AX
	POPF
lz4:	JMP	lz4	;(no memory: hangs)
lz5:	POP	AX
	POPF
	JNC	lz6
	MOV	BX,1
	STC
	RET
lz6:	MOV	BX,lzwDest
	RET
LoadZ	ENDP

LOADZ_TEXT	ENDS

END
