;
; Ultima VI - The False Prophet: U.EXE
; segment 0F15: DOS critical error (int 24h) and Ctrl-Break (int 23h) handlers
;
; Assembly (MASM 3.0, /Mx), not C: the prologue saves di before si and ds twice, and
; the handlers save only what they use, which Turbo C does not do.
;

_DATA	SEGMENT PUBLIC WORD 'DATA'
PUBLIC	critError
critError	dw 0	; D_410E set to 1 by the critical error handler
_DATA	ENDS

DGROUP	GROUP	_DATA

CRITERR_TEXT	SEGMENT PUBLIC WORD 'CODE'
	ASSUME	CS:CRITERR_TEXT,DS:DGROUP

;C_0F15_0006: install both handlers (they live in this code segment)
PUBLIC	_SetDosHandlers
_SetDosHandlers	PROC FAR	;0F15:0006
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
	PUSH	DS
	PUSH	CS
	POP	DS
	MOV	DX,OFFSET CritErrHandler
	MOV	AX,2524h	;set vector 24h
	INT	21h
	PUSH	CS
	POP	DS
	MOV	DX,OFFSET BreakHandler
	MOV	AX,2523h	;set vector 23h
	INT	21h
	POP	DS
	POP	DS
	POP	SI
	POP	DI
	POP	BP
	RET
_SetDosHandlers	ENDP

;C_0F15_0027: int 24h: note the error in D_410E and tell DOS to ignore it (al = 0)
CritErrHandler	PROC FAR
	STI
	PUSH	DS
	PUSH	BX
	MOV	BX,DGROUP
	MOV	DS,BX
	MOV	BX,OFFSET DGROUP:critError
	MOV	WORD PTR [BX],1
	XOR	AL,AL
	POP	BX
	POP	DS
	IRET
CritErrHandler	ENDP

;C_0F15_003B: int 23h: Ctrl-Break does nothing
BreakHandler	PROC FAR
	STI
	CLC
	IRET
BreakHandler	ENDP

CRITERR_TEXT	ENDS

END
