;----------------------------------------
;OSI library (U.EXE): the uninitialised data of its C modules FADE and TIMER
;(laid out here for the reason SRC_U/BSS.ASM gives: Turbo C orders _BSS by name)
;----------------------------------------
DGROUP	GROUP	_BSS
_BSS	SEGMENT WORD PUBLIC 'BSS'

PUBLIC	_D_681C,_D_681E,_D_6820,_D_6822,_D_6824,_D_6826,_D_6828
PUBLIC	_D_682A,_D_682E,_D_6832,_D_6836,_D_683A

;---- FADE.C ----
_D_681C	LABEL	WORD	;681C FADE_step: error term of each value
	ORG	$+2
_D_681E	LABEL	WORD	;681E its distance to go
	ORG	$+2
_D_6820	LABEL	WORD	;6820 its direction (+1/-1)
	ORG	$+2
_D_6822	LABEL	WORD	;6822 the largest distance: the number of steps
	ORG	$+2
_D_6824	LABEL	WORD	;6824 the steps left
	ORG	$+2
_D_6826	LABEL	WORD	;6826 FADE_in: the colors used by the picture
	ORG	$+2
_D_6828	LABEL	WORD	;6828 FADE_out: the same
	ORG	$+2
;---- TIMER.C ----
_D_682A	LABEL	DWORD	;682A ticks counted since TIM_install
	ORG	$+4
_D_682E	LABEL	DWORD	;682E called each tick
	ORG	$+4
_D_6832	LABEL	DWORD	;6832 the previous int 1Ch handler
	ORG	$+4
_D_6836	LABEL	DWORD	;6836 loops of TIM_calibrate in one tick
	ORG	$+4
_D_683A	LABEL	DWORD	;683A TIM_delay's loop counter
	ORG	$+4

_BSS	ENDS

END
