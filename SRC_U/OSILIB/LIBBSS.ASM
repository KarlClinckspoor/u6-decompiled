;----------------------------------------
;OSI library (U.EXE): the uninitialised data of its C modules FADE and TIMER
;(laid out here for the reason SRC_U/BSS.ASM gives: Turbo C orders _BSS by name)
;----------------------------------------
DGROUP	GROUP	_BSS
_BSS	SEGMENT WORD PUBLIC 'BSS'

PUBLIC	_fadeErr,_fadeDist,_fadeDir,_fadeSteps,_fadeLeft,_fadeInColors,_fadeOutColors
PUBLIC	_timerTicks,_timerHook,_oldInt1C,_loopsPerTick,_delayCount

;---- FADE.C ----
_fadeErr	LABEL	WORD	;D_681C FADE_step: error term of each value
	ORG	$+2
_fadeDist	LABEL	WORD	;D_681E its distance to go
	ORG	$+2
_fadeDir	LABEL	WORD	;D_6820 its direction (+1/-1)
	ORG	$+2
_fadeSteps	LABEL	WORD	;D_6822 the largest distance: the number of steps
	ORG	$+2
_fadeLeft	LABEL	WORD	;D_6824 the steps left
	ORG	$+2
_fadeInColors	LABEL	WORD	;D_6826 FADE_in: the colors used by the picture
	ORG	$+2
_fadeOutColors	LABEL	WORD	;D_6828 FADE_out: the same
	ORG	$+2
;---- TIMER.C ----
_timerTicks	LABEL	DWORD	;D_682A ticks counted since TIM_install
	ORG	$+4
_timerHook	LABEL	DWORD	;D_682E called each tick
	ORG	$+4
_oldInt1C	LABEL	DWORD	;D_6832 the previous int 1Ch handler
	ORG	$+4
_loopsPerTick	LABEL	DWORD	;D_6836 loops of TIM_calibrate in one tick
	ORG	$+4
_delayCount	LABEL	DWORD	;D_683A TIM_delay's loop counter
	ORG	$+4

_BSS	ENDS

END
