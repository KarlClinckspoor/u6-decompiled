/* replaces nothing: the SDL2 backend of the platform layer (plat.h).
 * The only file that includes SDL. The game runs on the main thread and calls plat_pump()
 * from every wait; this file turns SDL's events into BIOS keys and mouse state, presents
 * the 320x200 frame scaled (integer multiples and 4:3 by default), and plays the PC
 * speaker as a square wave from the audio callback. */
#include "../plat.h"

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- state ---- */
static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static PlatConfig cfg;
static const uint8_t *src_pixels;
static const uint8_t *src_palette;
static int dirty = 1;
static int quit_requested;
static uint32_t last_present;
static plat_key_hook key_hook;
static plat_draw_hook draw_hook;
static plat_pump_hook pump_hook;
static int in_pump_hook;
static uint32_t paused_at, paused_total;
static int paused;
static int headless;

/* keyboard buffer (BIOS) */
#define KEYBUF 32
static uint16_t keybuf[KEYBUF];
static int key_head, key_tail;
static uint16_t shift_flags;

/* mouse */
static int16_t mouse_x = 160, mouse_y = 100;
static uint16_t mouse_buttons;

/* speaker */
static SDL_AudioDeviceID audio_dev;
static volatile uint32_t speaker_hz;
static double speaker_phase;
static int audio_rate = 44100;

static FILE *log_file;

void plat_log_to(const char *path)
{
	if (log_file)
		fclose(log_file);
	log_file = path ? fopen(path, "w") : 0;
}

void plat_log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	if (log_file) {
		va_start(ap, fmt);
		vfprintf(log_file, fmt, ap);
		va_end(ap);
		fputc('\n', log_file);
		fflush(log_file);
	}
}

void plat_message(const char *message)
{
	fprintf(stderr, "%s\n", message);
	if (!headless)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, cfg.title ? cfg.title : "Ultima VI", message, win);
}

void plat_fatal(const char *message)
{
	fprintf(stderr, "u6: %s\n", message);
	if (win)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, cfg.title ? cfg.title : "Ultima VI", message, win);
	exit(1);
}

int plat_quit_requested(void) { return quit_requested; }

/* ---- audio ---- */
static void audio_cb(void *ud, Uint8 *stream, int len)
{
	int16_t *out = (int16_t *)stream;
	int n = len / 2, i;
	uint32_t hz = speaker_hz;
	int amp = cfg.volume * 60;	/* 0..6000 */
	(void)ud;
	for (i = 0; i < n; i++) {
		if (hz == 0 || hz > 20000) {
			out[i] = 0;
			continue;
		}
		speaker_phase += (double)hz / audio_rate;
		if (speaker_phase >= 1.0)
			speaker_phase -= (int)speaker_phase;
		out[i] = (int16_t)(speaker_phase < 0.5 ? amp : -amp);
	}
}

void plat_speaker(uint32_t hz) { speaker_hz = hz; }

/* ---- video ---- */
static void make_window(void)
{
	int s = cfg.scale < 1 ? 3 : (cfg.scale > 8 ? 8 : cfg.scale);
	int h = cfg.aspect ? 240 : 200;
	win = SDL_CreateWindow(cfg.title ? cfg.title : "Ultima VI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		320 * s, h * s, SDL_WINDOW_RESIZABLE | (cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if (!win)
		plat_fatal(SDL_GetError());
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!ren)
		ren = SDL_CreateRenderer(win, -1, 0);
	if (!ren)
		plat_fatal(SDL_GetError());
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
	SDL_ShowCursor(SDL_DISABLE);
}

void plat_apply_config(const PlatConfig *c)
{
	int s;
	cfg = *c;
	if (headless || !win)
		return;
	SDL_SetWindowFullscreen(win, cfg.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (!cfg.fullscreen) {
		s = cfg.scale < 1 ? 3 : (cfg.scale > 8 ? 8 : cfg.scale);
		SDL_SetWindowSize(win, 320 * s, (cfg.aspect ? 240 : 200) * s);
	}
	dirty = 1;
}

/* where the 320x200 picture goes in the window */
static SDL_Rect picture_rect(void)
{
	int ww, wh, w, h;
	double ar = cfg.aspect ? 4.0 / 3.0 : 320.0 / 200.0;
	SDL_Rect r;
	SDL_GetRendererOutputSize(ren, &ww, &wh);
	if (cfg.integer_scale) {
		int sx = ww / 320, sy = wh / (cfg.aspect ? 240 : 200), s = sx < sy ? sx : sy;
		if (s < 1)
			s = 1;
		w = 320 * s;
		h = (cfg.aspect ? 240 : 200) * s;
		if (w > ww || h > wh) {
			w = ww;
			h = (int)(ww / ar);
		}
	} else {
		w = ww;
		h = (int)(ww / ar);
		if (h > wh) {
			h = wh;
			w = (int)(wh * ar);
		}
	}
	r.x = (ww - w) / 2;
	r.y = (wh - h) / 2;
	r.w = w;
	r.h = h;
	return r;
}

static void present(void)
{
	static uint8_t frame[320 * 200];
	static uint8_t pal[768];
	uint32_t *px;
	int pitch, i;
	SDL_Rect dst;
	if (!src_pixels || !ren)
		return;
	memcpy(frame, src_pixels, sizeof frame);
	memcpy(pal, src_palette, sizeof pal);
	if (draw_hook)
		draw_hook(frame, pal);
	if (SDL_LockTexture(tex, NULL, (void **)&px, &pitch) == 0) {
		uint32_t lut[256];
		for (i = 0; i < 256; i++) {
			uint32_t r = pal[i * 3] & 63, g = pal[i * 3 + 1] & 63, b = pal[i * 3 + 2] & 63;
			lut[i] = 0xff000000u | ((r << 2 | r >> 4) << 16) | ((g << 2 | g >> 4) << 8) | (b << 2 | b >> 4);
		}
		for (i = 0; i < 200; i++) {
			uint32_t *row = (uint32_t *)((uint8_t *)px + i * pitch);
			const uint8_t *s = frame + i * 320;
			int x;
			for (x = 0; x < 320; x++)
				row[x] = lut[s[x]];
		}
		SDL_UnlockTexture(tex);
	}
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	dst = picture_rect();
	SDL_RenderCopy(ren, tex, NULL, &dst);
	SDL_RenderPresent(ren);
}

void plat_video_source(const uint8_t *pixels, const uint8_t *palette6)
{
	src_pixels = pixels;
	src_palette = palette6;
	dirty = 1;
}

void plat_video_dirty(void) { dirty = 1; }

int plat_video_capture(uint8_t *frame, uint8_t *palette6)
{
	if (!src_pixels)
		return -1;
	memcpy(frame, src_pixels, 320 * 200);
	memcpy(palette6, src_palette, 768);
	if (draw_hook)
		draw_hook(frame, palette6);
	return 0;
}

/* ---- keyboard: SDL scancodes to PC set-1 scan codes and BIOS words ---- */
static uint8_t scan_of(SDL_Scancode sc)
{
	switch (sc) {
	case SDL_SCANCODE_ESCAPE: return 0x01;
	case SDL_SCANCODE_1: return 0x02; case SDL_SCANCODE_2: return 0x03; case SDL_SCANCODE_3: return 0x04;
	case SDL_SCANCODE_4: return 0x05; case SDL_SCANCODE_5: return 0x06; case SDL_SCANCODE_6: return 0x07;
	case SDL_SCANCODE_7: return 0x08; case SDL_SCANCODE_8: return 0x09; case SDL_SCANCODE_9: return 0x0A;
	case SDL_SCANCODE_0: return 0x0B; case SDL_SCANCODE_MINUS: return 0x0C; case SDL_SCANCODE_EQUALS: return 0x0D;
	case SDL_SCANCODE_BACKSPACE: return 0x0E; case SDL_SCANCODE_TAB: return 0x0F;
	case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11; case SDL_SCANCODE_E: return 0x12;
	case SDL_SCANCODE_R: return 0x13; case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Y: return 0x15;
	case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17; case SDL_SCANCODE_O: return 0x18;
	case SDL_SCANCODE_P: return 0x19; case SDL_SCANCODE_LEFTBRACKET: return 0x1A; case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
	case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x1C;
	case SDL_SCANCODE_A: return 0x1E; case SDL_SCANCODE_S: return 0x1F; case SDL_SCANCODE_D: return 0x20;
	case SDL_SCANCODE_F: return 0x21; case SDL_SCANCODE_G: return 0x22; case SDL_SCANCODE_H: return 0x23;
	case SDL_SCANCODE_J: return 0x24; case SDL_SCANCODE_K: return 0x25; case SDL_SCANCODE_L: return 0x26;
	case SDL_SCANCODE_SEMICOLON: return 0x27; case SDL_SCANCODE_APOSTROPHE: return 0x28; case SDL_SCANCODE_GRAVE: return 0x29;
	case SDL_SCANCODE_BACKSLASH: return 0x2B;
	case SDL_SCANCODE_Z: return 0x2C; case SDL_SCANCODE_X: return 0x2D; case SDL_SCANCODE_C: return 0x2E;
	case SDL_SCANCODE_V: return 0x2F; case SDL_SCANCODE_B: return 0x30; case SDL_SCANCODE_N: return 0x31;
	case SDL_SCANCODE_M: return 0x32; case SDL_SCANCODE_COMMA: return 0x33; case SDL_SCANCODE_PERIOD: return 0x34;
	case SDL_SCANCODE_SLASH: return 0x35; case SDL_SCANCODE_KP_MULTIPLY: return 0x37; case SDL_SCANCODE_SPACE: return 0x39;
	case SDL_SCANCODE_F1: return 0x3B; case SDL_SCANCODE_F2: return 0x3C; case SDL_SCANCODE_F3: return 0x3D;
	case SDL_SCANCODE_F4: return 0x3E; case SDL_SCANCODE_F5: return 0x3F; case SDL_SCANCODE_F6: return 0x40;
	case SDL_SCANCODE_F7: return 0x41; case SDL_SCANCODE_F8: return 0x42; case SDL_SCANCODE_F9: return 0x43;
	case SDL_SCANCODE_F10: return 0x44; case SDL_SCANCODE_F11: return 0x57; case SDL_SCANCODE_F12: return 0x58;
	case SDL_SCANCODE_HOME: case SDL_SCANCODE_KP_7: return 0x47;
	case SDL_SCANCODE_UP: case SDL_SCANCODE_KP_8: return 0x48;
	case SDL_SCANCODE_PAGEUP: case SDL_SCANCODE_KP_9: return 0x49;
	case SDL_SCANCODE_KP_MINUS: return 0x4A;
	case SDL_SCANCODE_LEFT: case SDL_SCANCODE_KP_4: return 0x4B;
	case SDL_SCANCODE_KP_5: return 0x4C;
	case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_KP_6: return 0x4D;
	case SDL_SCANCODE_KP_PLUS: return 0x4E;
	case SDL_SCANCODE_END: case SDL_SCANCODE_KP_1: return 0x4F;
	case SDL_SCANCODE_DOWN: case SDL_SCANCODE_KP_2: return 0x50;
	case SDL_SCANCODE_PAGEDOWN: case SDL_SCANCODE_KP_3: return 0x51;
	case SDL_SCANCODE_INSERT: case SDL_SCANCODE_KP_0: return 0x52;
	case SDL_SCANCODE_DELETE: case SDL_SCANCODE_KP_PERIOD: return 0x53;
	default: return 0;
	}
}

/* ASCII the BIOS gives for a key with the shift state, or 0 */
static uint8_t ascii_of(SDL_Keycode k, SDL_Scancode sc, uint16_t mod)
{
	int shift = (mod & KMOD_SHIFT) != 0, ctrl = (mod & KMOD_CTRL) != 0, alt = (mod & KMOD_ALT) != 0;
	static const char shifted_digits[] = ")!@#$%^&*(";
	if (alt)
		return 0;
	switch (sc) {
	case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return ctrl ? 10 : 13;
	case SDL_SCANCODE_ESCAPE: return 27;
	case SDL_SCANCODE_BACKSPACE: return ctrl ? 127 : 8;
	case SDL_SCANCODE_TAB: return shift ? 0 : 9;
	case SDL_SCANCODE_SPACE: return 32;
	case SDL_SCANCODE_KP_MULTIPLY: return '*';
	case SDL_SCANCODE_KP_MINUS: return '-';
	case SDL_SCANCODE_KP_PLUS: return '+';
	default: break;
	}
	if (k >= 'a' && k <= 'z') {
		if (ctrl)
			return (uint8_t)(k - 'a' + 1);
		return (uint8_t)((shift ^ ((mod & KMOD_CAPS) != 0)) ? k - 32 : k);
	}
	if (ctrl)
		return 0;
	if (k >= '0' && k <= '9')
		return (uint8_t)(shift ? shifted_digits[k - '0'] : k);
	switch (k) {
	case '-': return shift ? '_' : '-';
	case '=': return shift ? '+' : '=';
	case '[': return shift ? '{' : '[';
	case ']': return shift ? '}' : ']';
	case ';': return shift ? ':' : ';';
	case '\'': return shift ? '"' : '\'';
	case '`': return shift ? '~' : '`';
	case '\\': return shift ? '|' : '\\';
	case ',': return shift ? '<' : ',';
	case '.': return shift ? '>' : '.';
	case '/': return shift ? '?' : '/';
	default: return 0;
	}
}

/* the BIOS word for a key press: scan<<8 | ascii; function keys with shift/ctrl/alt
 * have their own scan codes, letters with alt are scan<<8 */
static uint16_t bios_word(SDL_Keysym ks)
{
	uint8_t scan = scan_of(ks.scancode), a;
	uint16_t mod = ks.mod;
	if (!scan)
		return 0;
	if (scan >= 0x3B && scan <= 0x44) {	/* F1..F10 */
		if (mod & KMOD_ALT) return (uint16_t)((scan + 0x2D) << 8);
		if (mod & KMOD_CTRL) return (uint16_t)((scan + 0x23) << 8);
		if (mod & KMOD_SHIFT) return (uint16_t)((scan + 0x19) << 8);
		return (uint16_t)(scan << 8);
	}
	if (scan == 0x57 || scan == 0x58)
		return (uint16_t)(scan << 8);
	if (scan >= 0x47 && scan <= 0x53 && scan != 0x4A && scan != 0x4E) {
		int numpad_digit = ks.scancode >= SDL_SCANCODE_KP_1 && ks.scancode <= SDL_SCANCODE_KP_PERIOD && (mod & KMOD_NUM);
		if (numpad_digit) {
			static const char np[] = "789-456+1230.";
			return (uint16_t)(scan << 8 | (uint8_t)np[scan - 0x47]);
		}
		if (mod & KMOD_CTRL) {
			static const uint8_t ctl[13] = {0x77, 0x8D, 0x84, 0, 0x73, 0x8F, 0x74, 0, 0x75, 0x91, 0x76, 0x92, 0x93};
			return (uint16_t)(ctl[scan - 0x47] << 8);
		}
		return (uint16_t)(scan << 8);
	}
	a = ascii_of(ks.sym, ks.scancode, mod);
	if (mod & KMOD_ALT)
		return (uint16_t)(scan << 8);
	if (!a && !(scan == 0x0F && (mod & KMOD_SHIFT)))
		return 0;
	return (uint16_t)(scan << 8 | a);
}

static void push_key(uint16_t w)
{
	int next = (key_tail + 1) % KEYBUF;
	if (next == key_head)
		return;	/* the BIOS beeps and drops it */
	keybuf[key_tail] = w;
	key_tail = next;
}

void plat_push_key(uint16_t bios_key)
{
	if (key_hook && key_hook(bios_key, 1)) {
		key_hook(bios_key, 0);
		return;
	}
	push_key(bios_key);
}

int plat_key_available(void) { return key_head != key_tail; }

uint16_t plat_key_get(void)
{
	uint16_t w;
	if (key_head == key_tail)
		return 0;
	w = keybuf[key_head];
	key_head = (key_head + 1) % KEYBUF;
	return w;
}

uint16_t plat_key_shift(void) { return shift_flags; }

static void update_shift(void)
{
	SDL_Keymod m = SDL_GetModState();
	shift_flags = (uint16_t)(((m & KMOD_RSHIFT) ? 1 : 0) | ((m & KMOD_LSHIFT) ? 2 : 0) |
		((m & KMOD_CTRL) ? 4 : 0) | ((m & KMOD_ALT) ? 8 : 0));
}

/* ---- mouse ---- */
int plat_mouse_present(void) { return !headless; }

void plat_mouse_state(int16_t *x, int16_t *y, uint16_t *buttons)
{
	*x = mouse_x;
	*y = mouse_y;
	*buttons = mouse_buttons;
}

static void set_mouse_from_window(int wx, int wy)
{
	SDL_Rect r;
	int ox, oy, ww, wh, lw, lh;
	if (!ren)
		return;
	/* window coordinates to renderer output (high-DPI) */
	SDL_GetWindowSize(win, &ww, &wh);
	SDL_GetRendererOutputSize(ren, &lw, &lh);
	ox = ww ? wx * lw / ww : wx;
	oy = wh ? wy * lh / wh : wy;
	r = picture_rect();
	if (r.w <= 0 || r.h <= 0)
		return;
	ox = (ox - r.x) * 320 / r.w;
	oy = (oy - r.y) * 200 / r.h;
	mouse_x = (int16_t)(ox < 0 ? 0 : ox > 319 ? 319 : ox);
	mouse_y = (int16_t)(oy < 0 ? 0 : oy > 199 ? 199 : oy);
}

/* ---- the pump ---- */
void plat_pump(void)
{
	SDL_Event e;
	uint32_t now;
	if (headless) {
		if (pump_hook && !in_pump_hook) {
			in_pump_hook = 1;
			pump_hook();
			in_pump_hook = 0;
		}
		return;
	}
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			quit_requested = 1;
			break;
		case SDL_KEYDOWN: {
			uint16_t w;
			update_shift();
			if (e.key.keysym.scancode == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
				cfg.fullscreen = !cfg.fullscreen;
				plat_apply_config(&cfg);
				break;
			}
			w = bios_word(e.key.keysym);
			if (key_hook && key_hook(w ? w : (uint16_t)(scan_of(e.key.keysym.scancode) << 8), 1))
				break;
			if (w)
				push_key(w);
			break;
		}
		case SDL_KEYUP:
			update_shift();
			if (key_hook)
				key_hook((uint16_t)(scan_of(e.key.keysym.scancode) << 8), 0);
			break;
		case SDL_MOUSEMOTION:
			set_mouse_from_window(e.motion.x, e.motion.y);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: {
			uint16_t bit = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 0;
			set_mouse_from_window(e.button.x, e.button.y);
			if (e.type == SDL_MOUSEBUTTONDOWN)
				mouse_buttons |= bit;
			else
				mouse_buttons &= (uint16_t)~bit;
			break;
		}
		case SDL_WINDOWEVENT:
			dirty = 1;
			break;
		default:
			break;
		}
	}
	now = SDL_GetTicks();
	if (dirty && now - last_present >= 14) {	/* about 70 Hz at most */
		present();
		last_present = now;
		dirty = 0;
	}
	if (pump_hook && !in_pump_hook) {
		in_pump_hook = 1;
		pump_hook();
		in_pump_hook = 0;
	}
}

void plat_set_key_hook(plat_key_hook hook) { key_hook = hook; }
void plat_set_pump_hook(plat_pump_hook hook) { pump_hook = hook; }

void plat_clock_pause(int p)
{
	if (p && !paused) {
		paused = 1;
		paused_at = SDL_GetTicks();
	} else if (!p && paused) {
		paused = 0;
		paused_total += SDL_GetTicks() - paused_at;
	}
}

void plat_mouse_raw(int16_t *x, int16_t *y, uint16_t *buttons)
{
	plat_mouse_state(x, y, buttons);
}
void plat_set_draw_hook(plat_draw_hook hook) { draw_hook = hook; }

/* ---- time ---- */
uint32_t plat_ms(void)
{
	return (paused ? paused_at : SDL_GetTicks()) - paused_total;
}
uint32_t plat_real_ms(void) { return SDL_GetTicks(); }
void plat_sleep_ms(uint32_t ms) { SDL_Delay(ms); }

/* ---- files ---- */
const char *plat_home_dir(void)
{
	static char path[1024];
	char *p;
	if (path[0])
		return path;
	p = SDL_GetPrefPath(NULL, "u6port");
	if (p) {
		snprintf(path, sizeof path, "%s", p);
		SDL_free(p);
	} else {
		snprintf(path, sizeof path, "./");
	}
	return path;
}

const char *plat_base_dir(void)
{
	static char path[1024];
	char *p;
	if (path[0])
		return path;
	p = SDL_GetBasePath();
	snprintf(path, sizeof path, "%s", p ? p : "./");
	SDL_free(p);
	return path;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
const char *plat_choose_folder(const char *title)
{
	static char path[MAX_PATH];
	BROWSEINFOA bi;
	LPITEMIDLIST id;
	if (headless)
		return 0;
	CoInitialize(NULL);
	memset(&bi, 0, sizeof bi);
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	id = SHBrowseForFolderA(&bi);
	if (!id)
		return 0;
	if (!SHGetPathFromIDListA(id, path))
		path[0] = 0;
	CoTaskMemFree(id);
	return path[0] ? path : 0;
}
#else
const char *plat_choose_folder(const char *title)
{
	(void)title;
	return 0;	/* SDL2 has no folder picker: the port asks for --data instead */
}
#endif

/* ---- init ---- */
int plat_init(const PlatConfig *c)
{
	cfg = *c;
	if (cfg.volume <= 0 && cfg.volume != 0)
		cfg.volume = 50;
	if (cfg.hidden) {
		headless = 1;
		return SDL_Init(SDL_INIT_TIMER);
	}
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | (cfg.audio ? SDL_INIT_AUDIO : 0)) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return -1;
	}
	make_window();
	if (cfg.audio) {
		SDL_AudioSpec want, have;
		memset(&want, 0, sizeof want);
		want.freq = 44100;
		want.format = AUDIO_S16SYS;
		want.channels = 1;
		want.samples = 512;
		want.callback = audio_cb;
		audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
		if (audio_dev) {
			audio_rate = have.freq;
			SDL_PauseAudioDevice(audio_dev, 0);
		}
	}
	return 0;
}

void plat_shutdown(void)
{
	if (audio_dev)
		SDL_CloseAudioDevice(audio_dev);
	if (tex) SDL_DestroyTexture(tex);
	if (ren) SDL_DestroyRenderer(ren);
	if (win) SDL_DestroyWindow(win);
	SDL_Quit();
}
