/*
 * Ultima VI - The False Prophet: U.EXE
 * the OSI library: the timer tick (int 1Ch) and a calibrated delay loop
 */
#include <dos.h>

/*this module's variables: its _DATA (int D_49FC = 0;) and its _BSS, in this order
  (Turbo C puts the _BSS variables in the order of its hash of their names: the sum of
  their characters, it seems; these names would give 6832 6836 682A 683A 682E)*/
extern int D_49FC;/*installed*/
extern long D_682A;/*ticks counted since TIM_install*/
extern void (*D_682E)(void);/*called each tick*/
extern void interrupt (*D_6832)(void);/*the previous int 1Ch handler*/
extern long D_6836;/*loops of TIM_calibrate in one tick*/
extern long D_683A;/*TIM_delay's loop counter*/

/*the default tick callback*/
/*C_1482_000E*/static void TIM_nop(void) {
}

/*C_1482_000F*/TIM_setCallback(void (*f)(void)) {
	D_682E = f;
}

/*C_1482_001F*/TIM_resetCallback() {
	D_682E = TIM_nop;
}

/*C_1482_002A*/static void interrupt TIM_handler(void) {
	(*D_682E)();
	D_682A++;
	(*D_6832)();
}

/*C_1482_0055*/TIM_install() {
	if(D_49FC == 0) {
		D_49FC = 1;
		D_6832 = getvect(0x1c);
		D_682E = TIM_nop;
		setvect(0x1c, TIM_handler);
	}
	D_682A = 0;
}

/*C_1482_009C*/TIM_remove() {
	if(D_49FC) {
		setvect(0x1c, D_6832);
		D_49FC = 0;
	}
}

/*count the loops of one timer tick*/
/*C_1482_00BE*/TIM_calibrate() {
	long t = 3;

	D_6836 = 0;
	TIM_install();
	while(D_682A != 2)
		;
	while(D_682A != t)
		D_6836++;
	TIM_remove();
}

/*wait about ms milliseconds (55 a tick)*/
/*C_1482_0113*/TIM_delay(int ms) {
	long n;

	n = (ms / 55) * D_6836;
	D_683A = 0;
	while(D_683A != n)
		D_683A++;
}
