;----------------------------------------
;OSI library (U.EXE): waiting for the vertical retrace
;----------------------------------------
INCLUDE OSI.INC

DSegStart
EXTRN _pMouse:WORD	;_pMouse
EXTRN _nMouseSem:WORD	;_nMouseSem
DSegEnd

CSegStart	VSYNC_TEXT

;wait until the display is in its vertical retrace
FProc	_VID_inRetrace	;1368:0002
C_0002:
	MOV	AX,40h
	MOV	ES,AX
	MOV	DX,ES:[63h]	;0040:0063 Video controller base I/O port address
	ADD	DL,6	;input status register 1
C_000F:
	IN	AL,DX
	TEST	AL,8
	JZ	C_000F
	RET
_VID_inRetrace	ENDP

;wait for the beginning of a vertical retrace
FProc	_VID_waitRetrace	;1368:0015
	MOV	AX,40h
	MOV	ES,AX
	MOV	DX,ES:[63h]
	ADD	DL,6
C_0022:
	IN	AL,DX
	TEST	AL,8
	JNZ	C_0022
C_0027:
	IN	AL,DX
	TEST	AL,8
	JZ	C_0027
	RET
_VID_waitRetrace	ENDP

;VID_inRetrace when the mouse pointer is shown
FProc	VID_mouseRetrace	;1368:002D
	MOV	BX,_pMouse
	CMP	[BX].installed,0
	JZ	C_003F
	MOV	AX,_nMouseSem
	CMP	AX,0
	JGE	C_0002
C_003F:
	RET
VID_mouseRetrace	ENDP

;is the display in its vertical retrace (8) or not (0)
FProc	VID_retrace	;1368:0040
	MOV	AX,40h
	MOV	ES,AX
	MOV	DX,ES:[63h]
	ADD	DL,6
	IN	AL,DX
	AND	AL,8
	MOV	AH,0
	RET
VID_retrace	ENDP

CSegEnd	VSYNC_TEXT

END
