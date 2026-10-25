/* stand-in for Borland's dos.h (the parts the game uses) */
#ifndef PORT_DOS_H
#define PORT_DOS_H
#include <stdint.h>
#pragma pack(push, 1)
struct WORDREGS { uint16_t ax, bx, cx, dx, si, di, cflag, flags; };
struct BYTEREGS { uint8_t al, ah, bl, bh, cl, ch, dl, dh; };
union REGS { struct WORDREGS x; struct BYTEREGS h; };
struct SREGS { uint16_t es, cs, ss, ds; };
#pragma pack(pop)
int int86(int intno, union REGS *in, union REGS *out);
int int86x(int intno, union REGS *in, union REGS *out, struct SREGS *seg);
int intdos(union REGS *in, union REGS *out);
void segread(struct SREGS *seg);
void outportb(int port, unsigned char value);
unsigned char inportb(int port);
#endif
