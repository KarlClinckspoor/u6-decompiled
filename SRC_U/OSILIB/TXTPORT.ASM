;----------------------------------------
;OSI library (U.EXE): text window, select the current one
;----------------------------------------
INCLUDE OSI.INC

DSegStart
PUBLIC _pText
_pText	dw 0	;D_45B2 struct tText *: the current text window
DSegEnd

CSegStart	TXTPORT_TEXT

;TXT_select(struct tText *t)
ProcStart	TXT_select	;132B:0006
	MOV	AX,[BP+06]
	MOV	_pText,AX
ProcEnd	TXT_select

CSegEnd	TXTPORT_TEXT

END
