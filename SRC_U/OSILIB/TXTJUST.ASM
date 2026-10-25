;----------------------------------------
;OSI library (U.EXE): text window, printing a string word-wrapped and justified
;----------------------------------------
;TASM fixup order: 011D 019D 01CE 000C 00E6 00ED 00F4 00FB   (TASM wrote these fixups in this order: tools/u/tasmorder.py)
INCLUDE OSI.INC

DSegStart
EXTRN _pText:WORD	;struct tText *: the current text window

pStr	dd 0	;D_457C the string
strPos	dw 0	;D_4580 index of the next line
lineStart	dw 0	;D_4582 index of the line's first character
lineEnd	dw 0	;D_4584 index after the line's last word
lineWidth	dw 0	;D_4586 width in pixels of the line
lineRoom	dw 0	;D_4588 pixels from the cursor to the right of the window
spaceAdd	db 0	;D_458A full justification: pixels added to each space
spaceRem	db 0	;D_458B   and one more to the first spaceRem spaces
winRight	dw 0	;D_458C the window's right
winLeft	dw 0	;D_458E the window's left
mode	db 0	;D_4590 the justification (tText.justify)
DSegEnd

EXTRN TXT_putcAL:FAR	;print the character AL
EXTRN TXT_newLine:FAR	;new line

CSegStart	TXTJUST_TEXT

;print a string: TXT_puts(char far *s)
ProcStart	_TXT_puts	;128D:000C
	LES	DI,[BP+06]
	CALL	FAR PTR TXT_putsES
ProcEnd	_TXT_puts

;print the string at ES:DI, a line at a time: each line gets the words that fit in
;the window, and is printed in the window's justification
FProc	TXT_putsES	;128D:001F
	MOV	WORD PTR pStr,DI
	MOV	WORD PTR pStr+2,ES
	MOV	strPos,0
	MOV	BP,_pText
	MOV	AL,DS:[BP].justify
	MOV	mode,AL
	MOV	BX,DS:[BP].surface
	MOV	AX,[BX].right
	MOV	winRight,AX
	MOV	AX,[BX].left
	MOV	winLeft,AX
	LES	BX,pStr
C_004C:	;-- the first word always goes on the line
	MOV	SI,strPos
	MOV	lineStart,SI
	CMP	BYTE PTR ES:[BX+SI],0
	JZ	C_006B
C_005A:
	MOV	AL,ES:[BX+SI]
	INC	SI
	CMP	AL,0
	JZ	C_006B
	CMP	AL,0Ah
	JZ	C_006B
	CMP	AL,' '
	JNZ	C_005A
	DEC	SI
C_006B:
	MOV	lineEnd,SI
	;-- then the words while they fit
	MOV	CX,DS:[BP].curX
	MOV	SI,strPos
C_0077:
	MOV	AL,ES:[BX+SI]
	CMP	AL,0
	JZ	C_00A6
	CMP	AL,0Ah
	JZ	C_00A6
	CMP	AL,' '
	JNZ	C_008A
	MOV	lineEnd,SI
C_008A:
	INC	SI
	MOV	AH,0
	MOV	BX,AX
	LES	DI,DS:[BP].font
	ADD	CL,ES:[BX+DI+04]	;+ width of the character
	LES	BX,pStr
	ADC	CH,0
	CMP	CX,winRight
	JB	C_0077
	JMP	SHORT C_00AB
C_00A6:	;-- the end of the string (or of a line) fits
	INC	SI
	MOV	lineEnd,SI
C_00AB:	;-- the line's width
	MOV	AX,winRight
	SUB	AX,DS:[BP].curX
	INC	AX
	MOV	lineRoom,AX
	MOV	CX,0
	MOV	SI,lineStart
C_00BD:
	CMP	SI,lineEnd
	JNB	C_00DC
	MOV	AL,ES:[BX+SI]
	MOV	AH,0
	MOV	BX,AX
	LES	DI,DS:[BP].font
	ADD	CL,ES:[BX+DI+04]
	LES	BX,pStr
	ADC	CH,0
	INC	SI
	JMP	SHORT C_00BD
C_00DC:
	MOV	lineWidth,CX
	;-- print it
	MOV	AL,mode
	CMP	AL,0
	JZ	C_0104
	CMP	AL,1
	JZ	C_00FD
	CMP	AL,2
	JZ	C_00F6
	CALL	FAR PTR TXT_lineFull
	JMP	SHORT C_0109
C_00F6:
	CALL	FAR PTR TXT_lineCenter
	JMP	SHORT C_0109
C_00FD:
	CALL	FAR PTR TXT_lineRight
	JMP	SHORT C_0109
C_0104:
	CALL	FAR PTR TXT_lineLeft
C_0109:	;-- the next line
	MOV	SI,lineEnd
	LES	BX,pStr
	MOV	BP,_pText
	MOV	strPos,SI
	MOV	AL,ES:[BX+SI-01]
	CMP	AL,0
	JZ	C_0141
	CMP	AL,0Ah
	JZ	C_0141
	PUSH	AX
	CALL	TXT_newLine
	LES	BX,pStr
	MOV	SI,strPos
C_0133:	;-- skip the spaces
	CMP	BYTE PTR ES:[BX+SI],' '
	JNZ	C_013C
	INC	SI
	JMP	SHORT C_0133
C_013C:
	MOV	strPos,SI
	POP	AX
C_0141:
	CMP	AL,0
	JZ	C_0148
	JMPN	C_004C
C_0148:
	RET
TXT_putsES	ENDP

;centered line
FProc	TXT_lineCenter	;128D:0149
	MOV	SI,lineStart
	CMP	BYTE PTR ES:[BX+SI],0
	JZ	C_0190
	MOV	AX,lineWidth
	CMP	AX,lineRoom
	JA	C_0190
	MOV	AX,lineRoom
	SUB	AX,lineWidth
	INC	AX
	SHR	AX,1
	ADD	DS:[BP].curX,AX
	JMP	SHORT C_0190
TXT_lineCenter	ENDP

;line justified right
FProc	TXT_lineRight	;128D:016C
	MOV	SI,lineStart
	CMP	BYTE PTR ES:[BX+SI],0
	JZ	C_0189
	MOV	AX,lineWidth
	CMP	AX,lineRoom
	JA	C_0189
	MOV	AX,winRight
	SUB	AX,lineWidth
	INC	AX
	JMP	SHORT C_018C
C_0189:
	MOV	AX,winLeft
C_018C:
	MOV	DS:[BP].curX,AX
TXT_lineRight	ENDP

;line justified left: print the characters from lineStart to lineEnd
FProc	TXT_lineLeft	;128D:0190
C_0190:
	MOV	SI,lineStart
C_0194:
	LES	BX,pStr
	CMP	SI,lineEnd
	JNB	C_01AF
	MOV	AL,ES:[BX+SI]
	CMP	AL,0
	JZ	C_01AF
	PUSH	SI
	CALL	TXT_putcAL
	POP	SI
	INC	SI
	JMP	SHORT C_0194
C_01AF:
	RET
TXT_lineLeft	ENDP

;fully justified line: the room left is shared between the spaces (not the last line
;of a paragraph)
FProc	TXT_lineFull	;128D:01B0
	MOV	AX,lineWidth
	CMP	AX,lineRoom
	JNB	C_0190
	MOV	SI,lineEnd
	CMP	BYTE PTR ES:[BX+SI-01],0
	JZ	C_0190
	CMP	BYTE PTR ES:[BX+SI-01],0Ah
	JZ	C_0190
C_01CB:	;-- the leading spaces as they are
	MOV	SI,lineStart
	CMP	BYTE PTR ES:[BX+SI],' '
	JNZ	C_01F8
	MOV	AL,' '
	CALL	TXT_putcAL
	MOV	BP,_pText
	LES	DI,DS:[BP].font
	MOV	AL,ES:[DI+04+' ']
	LES	BX,pStr
	MOV	AH,0
	SUB	lineWidth,AX
	INC	lineStart
	JMP	SHORT C_01CB
C_01F8:	;-- the first word
	MOV	SI,lineStart
	MOV	AL,ES:[BX+SI]
	CMP	AL,' '
	JZ	C_0228
	PUSH	AX
	CALL	TXT_putcAL
	POP	BX
	MOV	BH,0
	MOV	BP,_pText
	LES	DI,DS:[BP].font
	MOV	AL,ES:[BX+DI+04]
	LES	BX,pStr
	MOV	AH,0
	SUB	lineWidth,AX
	INC	lineStart
	JMP	SHORT C_01F8
C_0228:	;-- the room left, and the spaces to share it
	MOV	DX,winRight
	SUB	DX,DS:[BP].curX
	INC	DX
	SUB	DX,lineWidth
	MOV	CX,0
	MOV	SI,lineStart
C_023C:
	CMP	SI,lineEnd
	JNB	C_024C
	CMP	BYTE PTR ES:[BX+SI],' '
	JNZ	C_0249
	INC	CX
C_0249:
	INC	SI
	JMP	SHORT C_023C
C_024C:
	CMP	CX,0
	JNZ	C_0254
	JMPN	C_0190
C_0254:
	MOV	AX,DX
	DIV	CL
	MOV	spaceRem,AH
	MOV	spaceAdd,AL
	MOV	SI,lineStart
C_0263:
	LES	BX,pStr
	CMP	SI,lineEnd
	JNB	C_02A0
	MOV	AL,ES:[BX+SI]
	CMP	AL,0
	JZ	C_02A0
	PUSH	SI
	PUSH	AX
	CALL	TXT_putcAL
	POP	AX
	CMP	AL,' '
	JNZ	C_029C
	MOV	BP,_pText
	MOV	AL,spaceAdd
	MOV	AH,0
	ADD	DS:[BP].curX,AX
	CMP	spaceRem,0
	JZ	C_029C
	DEC	spaceRem
	INC	DS:[BP].curX
C_029C:
	POP	SI
	INC	SI
	JMP	SHORT C_0263
C_02A0:
	RET
TXT_lineFull	ENDP

CSegEnd	TXTJUST_TEXT

END
