/* END.EXE in the port: runs its main. END.EXE links U.EXE's OSI library and its LoadZFile
 * (lzw.asm), whose ports (port/u) it shares; the graphics driver is MCGADRV.BIN's C
 * (port/u/ugfx.c), with U's mode set. */
#include "erun.h"
#include "../u/urun.h"
#include "../u/ugfx.h"

#include <stdio.h>
#include <string.h>

int E_main(int argc, char **argv);

int e_run(int argc, char **argv)
{
	static char config[80];
	char *args[6];
	int i;

	ugfx_set_mode_hook(u_mode_set);
	for (i = 0; i < argc && i < 5; i++)
		args[i] = argv[i];
	args[i] = 0;
	/* the configuration GAME.EXE passes, but VGA: the port has MCGADRV.BIN only */
	if (argc > 1) {
		snprintf(config, sizeof config, "%sV", argv[1]);
		args[1] = config;
	}
	return E_main(i, args);
}
