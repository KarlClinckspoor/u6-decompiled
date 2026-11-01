/* replaces nothing: the VGA DAC's palette registers (vgadac.h). */
#include "vgadac.h"
#include "../platform/plat.h"

uint8_t vga_palette[768];
static unsigned write_pos, read_pos;

void vga_dac_write_index(uint8_t index) { write_pos = (unsigned)index * 3; }

void vga_dac_write_data(uint8_t value)
{
	vga_palette[write_pos % 768] = value & 0x3f;
	write_pos = (write_pos + 1) % 768;
	plat_video_dirty();
}

void vga_dac_read_index(uint8_t index) { read_pos = (unsigned)index * 3; }

uint8_t vga_dac_read_data(void)
{
	uint8_t v = vga_palette[read_pos % 768];
	read_pos = (read_pos + 1) % 768;
	return v;
}
