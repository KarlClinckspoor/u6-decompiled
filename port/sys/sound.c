/* SOUND1-5.ASM: PC speaker sound.
 *
 * DOS: the speaker is driven by the PIT's channel 2 (port 42h: the divisor of 1193182 Hz)
 * gated by bits 0-1 of port 61h. OSI_sound(f) sets the divisor to 1234DEh / f and turns
 * the speaker on (if SoundFlag), OSI_nosound turns it off. The other routines time
 * themselves with the busy loop DELAY_CXAX calibrated by CpuSpeed (timer.c):
 *
 *   OSI_playNote(f, d)      OSI_sound(f) if SoundFlag, OSI_sDelay(1, d), OSI_nosound().
 *                           d units of CpuSpeed iterations: 1.318 ms each.
 *   OSI_playNoise(s, n, m)  speaker on; repeat: a new pseudo-random frequency in
 *                           [100, m] (own 16-bit generator, seed 7664h), wait s units of
 *                           CpuSpeed/16 iterations (82 us), until s*steps >= n (signed).
 *   OSI_playWavedNote(inc, tempo, count, threshold, sweep)
 *                           a 1-bit phase accumulator: the divisor is set to 3Ch (19.9 kHz,
 *                           a carrier above hearing) and for count steps: acc += inc,
 *                           speaker on if acc > threshold (unsigned) else off,
 *                           threshold += sweep, wait tempo * (CpuSpeed/24) iterations
 *                           (1/1000 tick = 54.9 us). The audible sound is the on/off
 *                           pattern: a pulse wave of frequency inc/65536 * the step rate
 *                           whose duty cycle sweeps. With SoundFlag off it only waits.
 *
 * Port: the speaker is plat_speaker(Hz): a square wave or silence, so these routines keep
 * the speaker's state (divisor, gate) and give plat_speaker the frequency it produces.
 * The waits are osi_spin_ns, in real time. Rapid changes cannot be rendered sample by
 * sample: a waved note is sent as its pulse wave frequency, refreshed every ~4 ms of steps
 * (silence for a stretch where the output never toggles: the carrier alone is
 * inaudible); a noise sends each new frequency, of which the backend hears those that
 * last long enough. Under replay the speaker stays silent and nothing waits.
 *
 * The arithmetic is the original's: 16-bit divisions that overflow (a frequency below 19
 * Hz, a range of 0 in OSI_playNoise, CpuSpeed >= 6144 in OSI_playWavedNote) were a CPU
 * divide error, which ends the DOS program: dos_divide_error does the same.
 */
#include "osi.h"
#include "dosint.h"
#include "../platform/plat.h"
#include "../replay/replay.h"

#define PIT_HZ 1193182u

static uint16_t pit_div;          /* channel 2 count; 0 = 65536 */
static int gate;                  /* port 61h bits 0-1 */
static uint32_t last_hz = 0xFFFFFFFFu;
static uint16_t noise_seed = 0x7664;   /* D_3098 */

void (*osi_sound_trace)(int what, uint16_t value);

static void out_hz(uint32_t hz)
{
	if (osi_replaying())
		hz = 0;
	if (hz != last_hz) {
		last_hz = hz;
		plat_speaker(hz);
	}
}

static void speaker_apply(void)
{
	out_hz(gate ? PIT_HZ / (pit_div ? pit_div : 0x10000u) : 0);
}

static void set_gate(int on)
{
	gate = on;
	if (osi_sound_trace)
		osi_sound_trace(1, (uint16_t)on);
	speaker_apply();
}

static void set_divisor(uint16_t d)
{
	pit_div = d;
	if (osi_sound_trace)
		osi_sound_trace(0, d);
	if (gate)
		speaker_apply();
}

/* SOUND_FREQ: DX:AX = 1234DEh divided by freq */
static uint16_t freq_divisor(uint16_t freq, const char *where)
{
	uint32_t q;

	if (freq == 0)
		dos_divide_error(where);
	q = 0x1234DEu / freq;
	if (q > 0xFFFFu)
		dos_divide_error(where);
	return (uint16_t)q;
}

void OSI_nosound(void)
{
	set_gate(0);
}

void OSI_sound(int frequency)
{
	if (SoundFlag == 0)
		return;
	set_divisor(freq_divisor((uint16_t)frequency, "OSI_sound"));
	set_gate(1);
}

/* SOUND5.ASM (seg_33E3.c is its C form) */
void OSI_playNote(int frequency, int duration)
{
	if ((uint8_t)SoundFlag)
		OSI_sound(frequency);
	OSI_sDelay(1, duration);
	OSI_nosound();
}

void OSI_playNoise(int step, int length, int maxFrequency)
{
	uint16_t acc = 0;   /* D_3092 */

	if ((uint8_t)SoundFlag)
		set_gate(1);
	do {
		uint16_t ax, cx, f;

		ax = (uint16_t)(noise_seed + 0x9248u);
		ax = (uint16_t)((ax >> 3) | (ax << 13));   /* ror ax,3 */
		ax ^= 0x9248u;
		ax = (uint16_t)(ax + 0x11u);
		noise_seed = ax;
		cx = (uint16_t)((uint16_t)maxFrequency - 100u + 1u);
		if (cx == 0)
			dos_divide_error("OSI_playNoise");
		f = (uint16_t)(ax % cx + 100u);
		set_divisor(freq_divisor(f, "OSI_playNoise"));
		acc = (uint16_t)(acc + (uint16_t)step);
		osi_spin_loops(osi_cxax_loops((uint16_t)step, (uint16_t)(CpuSpeed >> 4)));
	} while ((int16_t)acc < (int16_t)length);
	set_gate(0);
}

#define WAVE_WINDOW_NS 4000000u   /* refresh the waved note's frequency every ~4 ms */
#define WAVE_STEP_MIN_NS 5000u    /* a step without delay (CpuSpeed < 100): the loop itself */

void OSI_playWavedNote(int increment, int tempo, int count, int threshold, int sweep)
{
	uint16_t d3090, bx = (uint16_t)threshold, dx = 0, cx = (uint16_t)count;
	uint16_t inc = (uint16_t)increment;
	uint64_t step_ns, step_loops, hz_num;
	uint32_t window, n, edges;
	int on, prev;

	if ((int16_t)CpuSpeed >= 100) {
		uint16_t q = (uint16_t)(CpuSpeed / 24u);   /* div cl: AL = quotient */
		if (q > 0xFF)
			dos_divide_error("OSI_playWavedNote");
		d3090 = (uint16_t)(int16_t)(int8_t)(uint8_t)q;   /* cbw */
	} else {
		d3090 = 0;
	}
	set_divisor(0x3C);
	/* each step waits DELAY_CXAX(tempo, D_3090) when D_3090 != 0 */
	step_loops = d3090 ? osi_cxax_loops((uint16_t)tempo, d3090) : 0;
	step_ns = d3090 ? osi_loop_ns(step_loops) : WAVE_STEP_MIN_NS;
	if (step_ns == 0)
		step_ns = 1;
	window = (uint32_t)(WAVE_WINDOW_NS / step_ns);
	if (window == 0)
		window = 1;

	if ((uint8_t)SoundFlag == 0) {
		/* the same loop without sound: only the time passes (LOOP: 0 means 65536) */
		uint32_t steps = cx ? cx : 0x10000u;
		if (d3090)
			osi_spin_loops(step_loops * steps);
		else
			osi_spin_ns(step_ns * steps);
		return;
	}
	/* the pulse wave's frequency: inc/65536 cycles per step, folded at half the rate */
	hz_num = inc <= 0x8000u ? inc : 0x10000u - inc;
	prev = gate;
	do {
		edges = 0;
		for (n = 0; n < window; n++) {
			dx = (uint16_t)(dx + inc);
			on = dx > bx;
			if (on && !prev)
				edges++;
			prev = on;
			bx = (uint16_t)(bx + (uint16_t)sweep);
			gate = on;
			if (osi_sound_trace)
				osi_sound_trace(3, (uint16_t)on);
			if (--cx == 0)
				break;
		}
		if (osi_sound_trace)
			osi_sound_trace(2, (uint16_t)(edges > 0xFFFF ? 0xFFFF : edges));
		if (edges)
			out_hz((uint32_t)((hz_num * 1000000000u) / (step_ns * 65536u)));
		else
			out_hz(0);
		n = n < window ? n + 1 : window;   /* steps done */
		if (d3090)
			osi_spin_loops(step_loops * n);
		else
			osi_spin_ns(step_ns * n);
	} while (cx != 0);
	set_gate(0);
}
