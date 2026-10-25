;
; Ultima VI - The False Prophet: U.EXE
; segment 0F18: loading an LZW compressed (".z" style) file into memory
;
; Assembly (MASM 3.0, /Mx), not C. The decompressor is the one of GAME.EXE's
; SRC/OSILIB/INFLATE.ASM and LZ.INC (very close to Tom Pfau's code); here it is given a
; file name, opens the file itself and allocates the memory for the result.
; The file: a 4-byte length of the decompressed data, then the LZW codes (9 to 12 bits).
;

CLEAR      EQU 256	;code: reset the table
EOF        EQU 257	;code: end of the data
FIRST_FREE EQU 258

_DATA	SEGMENT PUBLIC WORD 'DATA'
PUBLIC	D_4110,D_4118,D_411A,D_411C,D_411E,D_4120,D_4122,D_4124,D_4126,D_4128
PUBLIC	D_412A,D_412C,D_412E,D_412F,D_4130,D_4132,D_4136,D_4138,D_413C
D_4110	dw 1FFh,3FFh,7FFh,0FFFh	;4110: masks: a code of 9..12 bits
D_4118	dw 0	;4118: input_handle
D_411A	dw 0	;411A: hash_seg: the string table (3 bytes a code)
D_411C	dw 0	;411C: output_offset
D_411E	dw 0	;411E: bit_offset: in the input buffer
D_4120	dw 0	;4120: cur_code
D_4122	dw 0	;4122: old_code
D_4124	dw 0	;4124: in_code
D_4126	dw 0	;4126: free_code
D_4128	dw 0	;4128: stack_count
D_412A	dw 0	;412A: max_code
D_412C	dw 0	;412C: nbits: the size of a code
D_412E	db 0	;412E: fin_char
D_412F	db 0	;412F: k
D_4130	dw 0	;4130: segment_0: a 3010h bytes buffer (the string table)
D_4132	dd 0	;4132: lDestSz: the decompressed size, from the file header
D_4136	dw 0	;4136: output_segment
D_4138	dw 0,0	;4138: lpDest: segment, offset of the result
D_413C	dw 0	;413C: input_segment: a 1010h bytes buffer (1 KB of input)
_DATA	ENDS

DGROUP	GROUP	_DATA

	EXTRN	_malloc:FAR
	EXTRN	_free:FAR
	EXTRN	_farmalloc:FAR

LZW_TEXT	SEGMENT PUBLIC WORD 'CODE'
	ASSUME	CS:LZW_TEXT,DS:DGROUP

;-- the next code from the input buffer, in ax (refills the buffer near its end)
read_code	MACRO
	LOCAL rd0,rd1,rd2
	PUSH	BP

	MOV	AX,D_411E	;bit offset
	ADD	AX,D_412C	;adjusted by the code size
	XCHG	AX,D_411E
	MOV	CX,8
	XOR	DX,DX
	DIV	CX
	CMP	AX,1021	;approaching the end of the buffer?
	JL	rd0	;no

	PUSH	DX	;offset in the byte
	ADD	DX,D_412C	;new bit offset
	MOV	D_411E,DX
	MOV	CX,1024	;1 KB buffer
	MOV	BP,AX	;byte offset
	SUB	CX,AX	;bytes left
	;-- move them to the start and read the rest
	PUSH	DS
	MOV	BX,D_4118
	MOV	DS,D_413C
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
	MOV	DS,D_413C
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
	MOV	BX,D_412C	;mask off the unwanted bits
	SUB	BX,9
	SHL	BX,1
	AND	AX,D_4110[BX]

	POP	BP
ENDM

init_tab	MACRO
	MOV	D_412C,9
	MOV	D_412A,512
	MOV	D_4126,FIRST_FREE
ENDM

;-- one byte (al) of output, normalising the far pointer every 16 bytes
write_char	MACRO
	LOCAL wc1

	PUSH	ES
	MOV	ES,D_4136
	MOV	DI,D_411C
	STOSB
	POP	ES
	INC	D_411C
	CMP	DI,10h
	JNZ	wc1
	INC	D_4136
	MOV	D_411C,0
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
	MOV	BX,D_4126
	index
	PUSH	ES
	MOV	ES,D_411A
	MOV	AL,D_412F	;suffix char
	MOV	ES:2[BX],AL
	MOV	AX,D_4122	;prefix code
	MOV	ES:[BX],AX
	POP	ES
	INC	D_4126
ENDM

;C_0F18_000E: void far *LoadZFile(char *name): the decompressed file in newly
;allocated memory, 0 if it cannot be read
PUBLIC	_LoadZFile
_LoadZFile	PROC FAR
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
	MOV	D_4130,AX
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
	MOV	D_413C,AX
	MOV	DX,[BP+6]	;file name
	CALL	FAR PTR LoadZ
	JC	lz_fail
	CALL	_free
	ADD	SP,2
	CALL	_free
	ADD	SP,2
	MOV	DX,D_4138
	MOV	AX,D_4138+2
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
	MOV	D_412C,9
	MOV	D_411E,0
	MOV	D_4126,FIRST_FREE
	MOV	D_412A,512

	MOV	AX,D_4130
	MOV	D_411A,AX

	MOV	BX,D_4118
	PUSH	DS
	MOV	DS,D_413C
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
	MOV	ES,D_411A
	MOV	DI,AX
	REPZ	STOSW

	RET	;done

L2:	CMP	AX,CLEAR
	JE	L3
	JMP	L7
L3:
	init_tab
	read_code
	MOV	D_4120,AX
	MOV	D_4122,AX
	MOV	D_412F,AL
	MOV	D_412E,AL
	write_char
	JMP	L1

L7:	MOV	D_4120,AX	;a new code
	MOV	D_4124,AX
	MOV	ES,D_411A
	CMP	AX,D_4126	;in the table? (k<w>k<w>k)
	JL	L11	;yes
	MOV	AX,D_4122	;the previous code
	MOV	D_4120,AX
	MOV	AL,D_412E	;its last char
	PUSH	AX
	INC	D_4128
L11:	CMP	D_4120,255	;code or char?
	JLE	L15	;char

	MOV	BX,D_4120
	index
	MOV	AL,ES:2[BX]	;suffix char
	PUSH	AX
	INC	D_4128
	MOV	AX,ES:[BX]	;prefix code
	MOV	D_4120,AX
	JMP	L11

L15:
	PUSH	DS
	POP	ES
	MOV	AX,D_4120
	MOV	D_412E,AL
	MOV	D_412F,AL
	PUSH	AX
	INC	D_4128
	MOV	CX,D_4128	;output the stacked chars
	JCXZ	L18
L17:	POP	AX
	write_char
	LOOP	L17
L18:	MOV	D_4128,CX
	add_code
	MOV	AX,D_4124
	MOV	D_4122,AX
	MOV	BX,D_4126	;table full?
	CMP	BX,D_412A
	JL	L23
	CMP	D_412C,12	;already twelve bits?
	JE	L23
	INC	D_412C	;longer codes
	SHL	D_412A,1
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
	MOV	D_4136,0
	MOV	D_411C,0
	MOV	D_4138,0
	MOV	D_4118,0
	MOV	AL,2
	MOV	AH,3Dh	;open file
	INT	21h
	MOV	BX,AX
	JC	lz1
	MOV	D_4118,BX
	;-- the header: the decompressed size
	MOV	CX,4
	MOV	DX,OFFSET DGROUP:D_4132
	MOV	AH,3Fh	;read file
	INT	21h
	JC	lz1
	MOV	BX,WORD PTR D_4132
	MOV	DX,WORD PTR D_4132+2
	MOV	CL,4
lz0:	SHR	DX,1
	RCR	BX,1
	LOOP	lz0
	INC	BX
	CALL	alloc_para
	JNC	lz2
	MOV	D_4136,-1
	JMP	SHORT lz1
lz2:	MOV	D_411C,AX
	MOV	D_4138+2,AX
	MOV	D_4136,DX
	MOV	D_4138,DX
	CALL	decompress
	CLC
	MOV	AX,WORD PTR D_4132
	OR	AX,AX
	JNZ	lz1
	DEC	AX
lz1:	PUSHF
	PUSH	AX
	MOV	BX,D_4118
	JZ	lz3
	MOV	AH,3Eh	;close file
	INT	21h
lz3:	MOV	BX,D_4136
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
lz6:	MOV	BX,D_4138
	RET
LoadZ	ENDP

LZW_TEXT	ENDS

END
