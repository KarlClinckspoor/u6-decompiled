/* --video FILE (tests): the session as a video, through ffmpeg (U6_FFMPEG, or ffmpeg on PATH).
 * One frame per tick of the game's clock (18.2 a second, the PIT's rate), taken when the port
 * pumps: under a replay that clock is virtual, so the video plays at the game's own pace however
 * fast the replay runs. It only reads the screen (plat_video_capture, the overlay included):
 * the game's state and the recording are the same with or without it. */
#include "video.h"
#include "osi.h"
#include "../platform/plat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *pipe_;
static uint32_t last_tick;
static int started;

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#define PIPE_MODE "wb"
#else
#define PIPE_MODE "w"
#endif

int video_open(const char *path)
{
	char cmd[2048];
	const char *ff = getenv("U6_FFMPEG");
	/* 1193182 / 65536 ticks a second; doubled in size, nearest neighbour, for players */
	snprintf(cmd, sizeof cmd,
#ifdef _WIN32
		/* _popen runs cmd /c, which drops the first and the last quote of a line that starts
		 * with one: the whole line in one more pair */
		"\""
#endif
		"\"%s\" -y -loglevel error -f rawvideo -pix_fmt rgb24 -s 320x200 -r 1193182/65536 -i - "
		"-vf scale=640:400:flags=neighbor -c:v libx264 -pix_fmt yuv420p -crf 18 \"%s\""
#ifdef _WIN32
		"\""
#endif
		, ff && *ff ? ff : "ffmpeg", path);
	pipe_ = popen(cmd, PIPE_MODE);
	if (!pipe_) {
		fprintf(stderr, "u6: --video: cannot run ffmpeg (set U6_FFMPEG)\n");
		return -1;
	}
	return 0;
}

static void write_frame(void)
{
	static uint8_t px[320 * 200], pal[768], rgb[320 * 200 * 3];
	int i;
	if (plat_video_frame(px, pal) != 0)	/* the game's screen, without the port's overlay */
		memset(px, 0, sizeof px), memset(pal, 0, sizeof pal);
	for (i = 0; i < 320 * 200; i++) {
		const uint8_t *c = pal + px[i] * 3;
		rgb[i * 3] = (uint8_t)(c[0] << 2 | c[0] >> 4);
		rgb[i * 3 + 1] = (uint8_t)(c[1] << 2 | c[1] >> 4);
		rgb[i * 3 + 2] = (uint8_t)(c[2] << 2 | c[2] >> 4);
	}
	fwrite(rgb, 1, sizeof rgb, pipe_);
}

static void frames_to(uint32_t now)
{
	uint32_t n;
	if (!started) {
		started = 1;
		last_tick = now;
		write_frame();
		return;
	}
	/* a frame for each tick since the last one (the screen as it is now: between pumps the
	 * game drew nothing the player could see either); a jump of over a minute (a clock set
	 * forward) gives one */
	n = now - last_tick;
	if (n > 18 * 60)
		n = 1;
	while (n--)
		write_frame();
	last_tick = now;
}

/* live play (--record, --keys): at the pumps, which come at host-timed moments */
void video_pump(void)
{
	if (pipe_ && !osi_replaying())
		frames_to(osi_ticks());
}

/* a replay: the virtual clock advances only in osi_wait_tick, at the same point of the game's
 * code in every run, so the same recording gives the same frames at any --speed */
void video_tick(void)
{
	if (pipe_)
		frames_to(osi_ticks());
}

void video_close(void)
{
	if (pipe_) {
		pclose(pipe_);
		pipe_ = 0;
	}
}
