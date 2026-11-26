;STACKRP.ASM: the stack check of the replay build only (PLAN-exact-port.md, sections 9 and
;Phase 1 task 6); the DOS build does not link it, and the gate never assembles this file.
;
;rpgame.h's RP_START calls _u6_stack_fill once, at the first hook call (main's first
;statement): it fills the free stack with zeros (what DOSBox-X's memory
;holds there anyway, and what the port's stack junk is: a fill of another value would change
;what the game's uninitialised locals read, and the replay build would no longer behave as
;GAME.EXE), from the near heap's break (C0's __brklvl,
;above the copies of the arguments and the environment that C0 keeps from _BSSEND, EDFAh, up)
;to 40 bytes below its caller's SP (32 below its own), and keeps the fill's start and its
;caller's SP in _u6_stack_info. Each dump's STAK section then asks _u6_stack_low for the lowest address
;written since: the first byte from the fill's start that is not 0 (a
;floor: a zero written by the game is not seen). That covers the game,
;the harness's hooks on top of it and the interrupts, which push onto whatever stack is in
;use when they come.
;
;Nothing here is in DGROUP: the two words are in this code segment, so that the replay build's
;DGROUP keeps the normal build's layout.

_DATA	SEGMENT PUBLIC WORD 'DATA'
EXTRN	___brklvl:WORD		;C0's near heap break (DS:009Eh)
_DATA	ENDS
DGROUP	GROUP _DATA

_STACKRP_TEXT	SEGMENT PUBLIC WORD 'CODE'
	ASSUME CS:_STACKRP_TEXT,DS:DGROUP

PUBLIC _u6_stack_info
_u6_stack_info	LABEL FAR
StkLo	dw 0			;the fill's start (0: no fill yet)
StkTop	dw 0			;the SP of _u6_stack_fill's caller

;void far cdecl u6_stack_fill(void)
PUBLIC _u6_stack_fill
_u6_stack_fill	PROC FAR
	PUSH	DI
	PUSH	ES
	MOV	AX,SP
	ADD	AX,8			;the caller's SP: DI, ES and the return address
	MOV	CS:StkTop,AX
	MOV	DI,___brklvl
	INC	DI
	AND	DI,0FFFEh
	MOV	CS:StkLo,DI
	MOV	CX,SP
	SUB	CX,32
	JB	SF_DONE
	SUB	CX,DI
	JBE	SF_DONE
	SHR	CX,1
	MOV	AX,SS
	MOV	ES,AX
	XOR	AX,AX
	CLD
	REP	STOSW
SF_DONE:
	POP	ES
	POP	DI
	RET
_u6_stack_fill	ENDP

;unsigned far cdecl u6_stack_low(void): the lowest stack address written since the fill,
;the fill's top (StkTop - 40) if none, 0 if there was no fill
PUBLIC _u6_stack_low
_u6_stack_low	PROC FAR
	PUSH	DI
	PUSH	ES
	XOR	AX,AX
	MOV	DI,CS:StkLo
	OR	DI,DI
	JZ	SL_DONE
	MOV	CX,CS:StkTop
	SUB	CX,40
	SUB	CX,DI
	JBE	SL_DONE
	MOV	AX,SS
	MOV	ES,AX
	XOR	AL,AL
	CLD
	REPE	SCASB
	MOV	AX,DI
	JZ	SL_DONE			;all 0: AX is the fill's top
	DEC	AX			;DI is one past the first byte written
SL_DONE:
	POP	ES
	POP	DI
	RET
_u6_stack_low	ENDP

_STACKRP_TEXT	ENDS

END
