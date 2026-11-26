/* io.h for the host: Borland's handle I/O header, which Exhume's runtime/replay/replay.c
 * includes (open, read, write, lseek, close, O_BINARY). MinGW has one of its own, which this
 * file stands in front of; elsewhere the calls are POSIX's. No game source includes it. */
#ifndef U6PORT_IO_H
#define U6PORT_IO_H
#ifdef _WIN32
#include_next <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif
#endif
