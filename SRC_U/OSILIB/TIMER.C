/*
 * Ultima VI - The False Prophet: U.EXE
 * the OSI library: the timer tick (int 1Ch) and a calibrated delay loop
 */
/*the library was compiled through the assembler (tcc -B): this does the same*/
#pragma inline
#include <dos.h>
#include "portable.h"

/*this module's variables: its _DATA (int timerInstalled = 0;) and its _BSS, in this order
  (Turbo C puts the _BSS variables in the order of its hash of their names: the sum of
  their characters, it seems; these names would give 6832 6836 682A 683A 682E)*/
/*49FC*/int16 timerInstalled = 0;/*installed*/
extern int32 timerTicks;/*ticks counted since TIM_install*/
extern void (*timerHook)(void);/*called each tick*/
extern void interrupt (*oldInt1C)(void);/*the previous int 1Ch handler*/
extern int32 loopsPerTick;/*loops of TIM_calibrate in one tick*/
extern int32 delayCount;/*TIM_delay's loop counter*/

/*the default tick callback*/
/*C_1482_000E*/static void TIM_nop(void) {
}

/*C_1482_000F*/TIM_setCallback(void (*f)(void)) {
	timerHook = f;
}

/*C_1482_001F*/TIM_resetCallback() {
	timerHook = TIM_nop;
}

/*C_1482_002A*/static void interrupt TIM_handler(void) {
	(*timerHook)();
	timerTicks++;
	(*oldInt1C)();
}

/*C_1482_0055*/TIM_install() {
	if(timerInstalled == 0) {
		timerInstalled = 1;
		oldInt1C = getvect(0x1c);
		timerHook = TIM_nop;
		setvect(0x1c, TIM_handler);
	}
	timerTicks = 0;
}

/*C_1482_009C*/TIM_remove() {
	if(timerInstalled) {
		setvect(0x1c, oldInt1C);
		timerInstalled = 0;
	}
}

/*count the loops of one timer tick*/
/*C_1482_00BE*/TIM_calibrate() {
	int32 t = 3;

	loopsPerTick = 0;
	TIM_install();
	while(timerTicks != 2)
		;
	while(timerTicks != t)
		loopsPerTick++;
	TIM_remove();
}

/*wait about ms milliseconds (55 a tick)*/
/*C_1482_0113*/TIM_delay(int16 ms) {
	int32 n;

	n = (ms / 55) * loopsPerTick;
	delayCount = 0;
	while(delayCount != n)
		delayCount++;
}
