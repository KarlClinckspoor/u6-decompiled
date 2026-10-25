/*
 * Ultima VI - The False Prophet: U.EXE
 * the OSI library: surfaces sharing a bitmap
 */
#include <stdarg.h>
#include "OSILIB.H"

/*give the surfaces of the list (ended by NULL) the bitmap of the first one:
  SURF_share(src, s1, s2, ..., NULL)*/
/*C_12EA_000A*/SURF_share(struct tSurface *src, ...) {
	va_list ap;
	register struct tSurface *s;

	va_start(ap, src);
	while((s = va_arg(ap, struct tSurface *)) != 0) {
		s->segment = src->segment;
		s->scanlines = src->scanlines;
	}
}
