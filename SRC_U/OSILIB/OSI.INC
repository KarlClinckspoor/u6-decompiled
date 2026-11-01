;OSI library (U.EXE version): common macros
;(the procedures of this version save DI, SI, DS in this order: see ProcStart)

DSegStart	MACRO
_DATA	SEGMENT PUBLIC WORD 'DATA'
ENDM

DSegEnd	MACRO
_DATA	ENDS
ENDM

DGROUP	GROUP _DATA

DSegStart
DSegEnd


CSegStart	MACRO segName
segName	SEGMENT PUBLIC WORD 'CODE'
	ASSUME CS:segName,DS:DGROUP
ENDM

CSegEnd	MACRO segName
segName	ENDS
ENDM

;cdecl far procedure with a frame
ProcStart	MACRO procName
PUBLIC procName
procName	PROC FAR
	PUSH	BP
	MOV	BP,SP
	PUSH	DI
	PUSH	SI
	PUSH	DS
ENDM

ProcRet	MACRO
	POP	DS
	POP	SI
	POP	DI
	POP	BP
	RET
ENDM

ProcEnd	MACRO	procName
	ProcRet
procName	ENDP
ENDM

;far procedure without a frame
FProc	MACRO procName
PUBLIC procName
procName	PROC FAR
ENDM

;near JMP to a label of the same segment: MASM 3.0 leaves a self-relative fixup for a
;forward near jump (the bytes are the same; tools/u/ucmp.py takes such a fixup for an
;absolute offset): JMPN emits the jump without one
JMPN	MACRO target
	db	0E9h
	dw	target-$-2
ENDM

;a drawing surface: a bitmap in memory (or the screen) with its clipping window
tSurface	STRUC
	segment_	dw ?	;+00 segment of the pixels
	pScanlines	dd ?	;+02 offsets of the scanlines (one word per line)
	right	dw ?	;+06 clipping window (inclusive)
	left	dw ?	;+08
	top	dw ?	;+0A
	bottom	dw ?	;+0C
tSurface	ENDS

;a text window: where and how the text routines print
tText	STRUC
	surface	dw ?	;+00 struct tSurface *
	curX	dw ?	;+02 cursor
	curY	dw ?	;+04
	font	dd ?	;+06 the font: height (word), ..., width of each character at +4
	fgColor	db ?	;+0A
	bgColor	db ?	;+0B
	buffer	dw ?	;+0C string buffer of the formatted output
	bufPtr	dw ?	;+0E where the next character goes
	justify	db ?	;+10 0: ... 3: (see TXT_putJustified)
tText	ENDS

;the mouse (as in GAME.EXE's MOUSE.ASM)
tMouse	STRUC
	buttonL	db ?	;+00 left button down
	buttonR	db ?	;+01 right button down
	posX	dw ?	;+02
	posY	dw ?	;+04
	unused06	dw ?	;+06
	shapes	dd ?	;+08 the pointer shapes
	shapeIndex	dw ?	;+0C the shape shown
	mSurface	dw ?	;+0E struct tSurface * it is drawn on
	saveBuf	dd ?	;+10 the background under it
	onMove	dd ?	;+14 called when it moves or a button changes
	installed	db ?	;+18
tMouse	ENDS

;far JMP to a procedure of the same segment (MASM 3.0 turns JMP FAR PTR into a short
;or near jump padded with NOPs)
JMPF	MACRO target
	db	0EAh
	dd	target
ENDM
