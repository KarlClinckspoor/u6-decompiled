/* replaces the music drivers (U6ADLIB.DRV, U6ROLAND.DRV, ...): not ported yet, so every
 * entry does nothing and the game plays no music. The game's own music code (music.c)
 * still runs: it loads the driver file and the songs as in DOS. */
#include <stdint.h>

void MUSDRV_0(int port) { (void)port; }                       /* init(i/o port) */
void MUSDRV_6(void *song) { (void)song; }                     /* play(song data) */
void MUSDRV_9(void) { }                                       /* stop */
void MUSDRV_0xc(int channel, int a, int b) { (void)channel; (void)a; (void)b; } /* channel setup */
