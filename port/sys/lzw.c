/* INFLATE.ASM + LZ.INC: decompress(), the LZW unpacker (close to Tom Pfau's LZW code).
 *
 * decompress(fd, offset, dest, workSeg) seeks fd to offset, reads the 4-byte unpacked
 * size (only used to compute a never used block size), and unpacks the stream behind it
 * into dest. Codes are 9 to 12 bits, LSB first; 256 clears the table, 257 ends.
 *
 * The work segment is used exactly as the original does, in the DOS arena:
 *   workSeg:0000..2FFF   the table, 3 bytes a code: prefix code (word), suffix byte;
 *                        entry n at offset n*3 (16-bit arithmetic: if a stream ever went
 *                        past code 4095 without a clear, its entries would run into the
 *                        input buffer, as in DOS)
 *   (workSeg+300h):0000..03FF  the 1 KB input buffer: filled by a 1024-byte read; when
 *                        the next code starts at byte 1021 or later, the unread tail is
 *                        moved to the front and the buffer refilled behind it with a read
 *                        of as many bytes as were consumed (short reads at the end of the
 *                        file leave old bytes in place)
 * and at the end code the table (0x1800 words) is cleared, leaving the input buffer.
 * The file is read with plain DOS reads (no OSI error handler) and is left positioned
 * after the last block read. The output is written byte after byte from dest (the
 * original normalised dest and advanced the segment every 16 bytes, which is the same
 * memory), so an output overlapping the work area behaves as in DOS too.
 * The decoder's stack was the CPU stack; here it is an array.
 */
#include "osi.h"
#include "dosfs.h"
#include "dosmem.h"
#include "../platform/plat.h"

#define CLEAR 256
#define EOFCODE 257
#define FIRST_FREE 258

static const uint16_t masks[4] = {0x1FF, 0x3FF, 0x7FF, 0xFFF};

typedef struct {
	int16_t fd;
	uint16_t hashSeg, inSeg;
	uint16_t bit_offset, nbits, max_code, free_code;
	uint16_t cur_code, old_code, in_code;
	uint8_t fin_char, k;
	uint8_t *out;
} Lz;

static uint8_t stack_buf[0x10000];   /* PUSH AX per character; 64 KB of SS in DOS */

static void dos_read(Lz *z, uint16_t off, uint16_t count)
{
	uint16_t done;
	uint32_t lin = (uint32_t)z->inSeg * 16u + off;

	/* the input buffer never crosses its segment (offsets < 0x400) */
	dosfs_read(z->fd, dos_base + lin, count, &done);
}

static uint16_t read_code(Lz *z)
{
	uint16_t ax = z->bit_offset, byteoff, bitin, bx;
	uint8_t *in;
	uint32_t v;

	z->bit_offset = (uint16_t)(ax + z->nbits);
	byteoff = ax / 8;
	bitin = ax % 8;
	if ((int16_t)byteoff >= 1021) {
		uint16_t left = (uint16_t)(1024 - byteoff);
		in = dos_seg_ptr(z->inSeg, 0);
		z->bit_offset = (uint16_t)(bitin + z->nbits);
		/* REP MOVSB: forward copy */
		{
			uint16_t i;
			for (i = 0; i < left; i++)
				in[i] = in[byteoff + i];
		}
		dos_read(z, left, byteoff);
		byteoff = 0;
	}
	in = dos_seg_ptr(z->inSeg, 0);
	v = (uint32_t)in[byteoff] | ((uint32_t)in[byteoff + 1] << 8) | ((uint32_t)in[byteoff + 2] << 16);
	bx = (uint16_t)(v >> bitin);
	return (uint16_t)(bx & masks[(z->nbits - 9) & 3]);
}

static void write_char(Lz *z, uint8_t c)
{
	*z->out++ = c;
}

static uint8_t get_suffix(Lz *z, uint16_t code)
{
	return *dos_seg_ptr(z->hashSeg, (uint16_t)(code * 3u + 2u));
}

static uint16_t get_word(Lz *z, uint16_t code)
{
	uint16_t off = (uint16_t)(code * 3u);
	return (uint16_t)(*dos_seg_ptr(z->hashSeg, off) | (*dos_seg_ptr(z->hashSeg, off + 1) << 8));
}

static void add_code(Lz *z)
{
	uint16_t off = (uint16_t)(z->free_code * 3u);
	*dos_seg_ptr(z->hashSeg, off + 2) = z->k;
	*dos_seg_ptr(z->hashSeg, off) = (uint8_t)z->old_code;
	*dos_seg_ptr(z->hashSeg, off + 1) = (uint8_t)(z->old_code >> 8);
	z->free_code++;
}

/* LZ.INC's decompress proc */
static void lz_run(Lz *z)
{
	uint32_t sp = 0;   /* stack_count; the characters are in stack_buf[0..sp) */
	uint16_t ax;

	z->nbits = 9;
	z->bit_offset = 0;
	z->free_code = FIRST_FREE;
	z->max_code = 512;
	dos_read(z, 0, 1024);
	for (;;) {
		ax = read_code(z);
		if (ax == EOFCODE) {
			uint16_t i;
			for (i = 0; i < 0x3000; i++)
				*dos_seg_ptr(z->hashSeg, i) = 0;
			return;
		}
		if (ax == CLEAR) {
			z->nbits = 9;
			z->max_code = 512;
			z->free_code = FIRST_FREE;
			ax = read_code(z);
			z->cur_code = z->old_code = ax;
			z->k = z->fin_char = (uint8_t)ax;
			write_char(z, (uint8_t)ax);
			continue;
		}
		z->cur_code = z->in_code = ax;
		if ((int16_t)ax >= (int16_t)z->free_code) {
			/* KwKwK: the code is not in the table yet */
			z->cur_code = z->old_code;
			stack_buf[sp++] = z->fin_char;
		}
		while ((int16_t)z->cur_code > 255) {
			stack_buf[sp++] = get_suffix(z, z->cur_code);
			z->cur_code = get_word(z, z->cur_code);
			if (sp >= sizeof stack_buf - 1) {
				/* only a corrupt stream loops here; DOS would overrun its stack */
				plat_log("decompress: corrupt LZW data (decoder stack overflow)");
				return;
			}
		}
		z->fin_char = z->k = (uint8_t)z->cur_code;
		stack_buf[sp++] = z->k;
		while (sp)
			write_char(z, stack_buf[--sp]);
		add_code(z);
		z->old_code = z->in_code;
		if ((int16_t)z->free_code >= (int16_t)z->max_code && z->nbits != 12) {
			z->nbits++;
			z->max_code = (uint16_t)(z->max_code << 1);
		}
	}
}

int decompress(int16_t fd, uint32_t offset, void *dest, uint16_t workSeg)
{
	Lz z;
	uint8_t hdr[4];
	uint16_t done;

	if (!dos_base)
		dosmem_init();
	z.fd = fd;
	z.hashSeg = workSeg;
	z.inSeg = (uint16_t)(workSeg + 0x300);
	z.out = (uint8_t *)dest;
	z.cur_code = z.old_code = z.in_code = 0;
	z.fin_char = z.k = 0;
	/* int 21h 4200h with CX:DX = offset; its error is not checked */
	dosfs_seek(fd, 0, (int32_t)offset, 0);
	/* the Z-file header: the unpacked size */
	if (dosfs_read(fd, hdr, 4, &done) != 0) {
		/* the original returns through a near RET here and crashes */
		plat_log("decompress: cannot read handle %d (the DOS game would crash)", fd);
		return 0;
	}
	lz_run(&z);
	return dos_fp_off(dest);
}
