/* The AdLib music driver in C (adlib.c): the MUSDRV_* entries are declared in compat.h */
#ifndef PORT_ADLIB_H
#define PORT_ADLIB_H
void adlib_set_volume(int volume);	/* 0-100 */
void MUSDRV_9(void);
#endif
