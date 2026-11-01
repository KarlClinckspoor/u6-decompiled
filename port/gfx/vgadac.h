/* The VGA DAC (palette) shared by the graphics driver's C and the game's direct port
 * writes (outportb 3C8h/3C9h in the palette cycling): 256 entries of 6-bit r, g, b, the
 * layout plat_video_source takes. */
#ifndef VGADAC_H
#define VGADAC_H

#include <stdint.h>

extern uint8_t vga_palette[768];

void vga_dac_write_index(uint8_t index);   /* port 3C8h */
void vga_dac_write_data(uint8_t value);    /* port 3C9h: r, g, b in turn, then the next entry */
void vga_dac_read_index(uint8_t index);    /* port 3C7h */
uint8_t vga_dac_read_data(void);           /* port 3C9h read */

#endif
