/*
 * Ultima VI - The False Prophet: U.EXE
 * the OSI library: formatted output to the text window
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "OSILIB.H"

extern struct tText *D_45B2;/*the current text window*/
extern C_128D_000C(char far *s);/*TXT_puts: print a string, word-wrapped and justified*/
extern C_135A_0008(int c);/*TXT_putc: print a character*/
extern C_135A_001B(int c);/*TXT_bufc: add a character to the window's string buffer*/

/*output a string, a character at a time*/
/*C_12EE_0002*/static outstr(void (*out)(int), register char *s) {
	register int c;

	while((c = *s++) != 0)
		(*out)(c);
}

/*the formatter: %d %u %D (long) %U (unsigned long) %x (upper case hex) %c %s, and
  %X %Y: move the cursor, %F %B: foreground, background color, %P: print the window's
  string buffer*/
/*C_12EE_0024*/static format(void (*out)(int), char far *fmt, register va_list ap) {
	int c2;
	char buf[64];
	register int c;

	while((c = *fmt++) != 0) {
		if(c == '%') {
			c2 = *fmt++;
			switch(c2) {
				case 'd': outstr(out, itoa(va_arg(ap, int), buf, 10)); break;
				case 'u': outstr(out, ultoa(va_arg(ap, unsigned), buf, 10)); break;
				case 'D': outstr(out, ltoa(va_arg(ap, long), buf, 10)); break;
				case 'U': outstr(out, ultoa(va_arg(ap, unsigned long), buf, 10)); break;
				case 'x': outstr(out, strupr(ultoa(va_arg(ap, unsigned), buf, 16))); break;
				case 'c': (*out)(va_arg(ap, int)); break;
				case 's': outstr(out, va_arg(ap, char *)); break;
				case 'X': D_45B2->curX = va_arg(ap, int); break;
				case 'Y': D_45B2->curY = va_arg(ap, int); break;
				case 'B': D_45B2->bgColor = va_arg(ap, int); break;
				case 'F': D_45B2->fgColor = va_arg(ap, int); break;
				case 'P': C_128D_000C(D_45B2->buffer); break;
				default: (*out)(c2); break;
			}
		} else {
			(*out)(c);
		}
	}
}

/*print to the text window*/
/*C_12EE_0229*/TXT_printf(char far *fmt, ...) {
	register va_list ap;

	va_start(ap, fmt);
	format(C_135A_0008, fmt, ap);
}

/*format into the window's string buffer*/
/*C_12EE_0249*/TXT_sprintf(char far *fmt, ...) {
	register va_list ap;

	va_start(ap, fmt);
	D_45B2->bufPtr = D_45B2->buffer;
	format(C_135A_001B, fmt, ap);
}

/*and add to it*/
/*C_12EE_0277*/TXT_sappend(char far *fmt, ...) {
	register va_list ap;

	va_start(ap, fmt);
	format(C_135A_001B, fmt, ap);
}
