;DELAYRP.ASM: DELAY.ASM for the replay build only (PLAN-exact-port.md, Phase 1 task 5);
;the DOS build links DELAY.ASM, and the gate never assembles this file.
;
;OSI_delay is a frame limiter: the int 1Ch handler counts ticks and OSI_delay(n)
;waits until n have passed since the last call, then starts the count again. It
;decides nothing in the game, but DELAY.ASM keeps the count in DGROUP (D_305A),
;where the timer changes it behind the game's back, so two runs of one recording
;would dump different bytes there. Here the handler counts in a word of this code
;segment instead, and D_305A stays 0 for good. OSI_delay still stores n in D_305C
;as the original does.
;
;The data segment is DELAY.ASM's, byte for byte (the same names, sizes and order),
;so that DGROUP does not move; only the code differs.

INCLUDE OSI.INC

EXTRN CpuSpeed:WORD	;computer speed

DSegStart	
	
D_305A	dw 0	;0 for good in the replay build
D_305C	dw 0
D_305E	dw 0,0	;original int 1Ch

DSegEnd


CSegStart	_DELAY_TEXT

;the tick count, outside DGROUP (D_305A's job in DELAY.ASM); before the code,
;so that MASM knows it at its first use
Ticks	dw 0

ProcStart	OSI_delay
	MOV	AX,[BP+06]
	MOV	D_305C,AX
C_000E:	;--
	MOV	AX,CS:Ticks
	CMP	AX,D_305C
	JB	C_000E
	;--
	MOV	CS:Ticks,0
ProcEndN	OSI_delay,2


NEW_INT_1C:
	STI
	INC	CS:Ticks
	IRET

PUBLIC OSI_delay_init
OSI_delay_init	PROC FAR
	PUSH	SI
	PUSH	DI
	PUSH	DS
	;--
	MOV	CS:Ticks,0
	;--
	PUSH	ES
	MOV	AL,1Ch
	MOV	AH,35h
	INT	21h
	MOV	AX,ES
	POP	ES
	MOV	D_305E,BX
	MOV	D_305E+2,AX
	;--
	PUSH	DS
	PUSH	CS
	POP	DS
	LEA	DX,NEW_INT_1C
	MOV	AL,1Ch
	MOV	AH,25h
	INT	21h
	POP	DS
	;--
	POP	DS
	POP	DI
	POP	SI
	RET
OSI_delay_init	ENDP

PUBLIC OSI_delay_clean
OSI_delay_clean	PROC FAR
	PUSH	SI
	PUSH	DI
	PUSH	DS
	;--
	MOV	AX,D_305E+2
	MOV	DX,D_305E
	PUSH	DS
	MOV	DS,AX
	MOV	AL,1Ch
	MOV	AH,25h
	INT	21h
	POP	DS
	;--
	POP	DS
	POP	DI
	POP	SI
	RET
OSI_delay_clean	ENDP

CSegEnd	_DELAY_TEXT

END
