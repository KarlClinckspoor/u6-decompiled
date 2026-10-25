/*
	OSI sound library: OSI_playNote.
	(the build uses the assembly version SOUND5.ASM in OSISND.LIB; the
	original was most likely this C code)
*/
#include "u6.h"

/*play a PC speaker tone "frequency" (if sound is on) for "duration"
  delay units (OSI_sDelay speed table entry 1, calibrated on CpuSpeed)*/
/*C_33E3_0008*/OSI_playNote(int frequency, int duration) {
	if((char)SoundFlag)
		OSI_sound(frequency);
	OSI_sDelay(1, duration);
	OSI_nosound();
}

/*__emit__(0x90);*/
