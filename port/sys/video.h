/* --video FILE (tests): the session as a video through ffmpeg (video.c) */
#ifndef PORT_VIDEO_H
#define PORT_VIDEO_H
int video_open(const char *path);
void video_pump(void);      /* live, at every pump: the frames of the ticks since the last call */
void video_tick(void);      /* in a replay, when the virtual clock advances (timer.c) */
void video_close(void);
#endif
