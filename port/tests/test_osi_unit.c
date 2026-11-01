/* Unit tests of port/sys: the DOS file system layer (dosfs), the OSI file error protocol,
 * OSI_delay and the timer (simulated clock, record and replay), dosmem and dosint.
 * Built and run by run_osilib_tests.sh; POSIX host (temporary folders, fork). */
#define _POSIX_C_SOURCE 200809L
#include "../sys/osi.h"
#include "../sys/dosfs.h"
#include "../sys/dosmem.h"
#include "../sys/dosint.h"
#include "../replay/replay.h"
#include "../gfx/vgadac.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* osi_stubs.c */
void stub_set_fake_clock(int on, uint32_t ms);
void stub_key_push(uint16_t k);
void stub_key_clear(void);
void stub_quiet_log(int q);
void stub_set_dirs(const char *home, const char *base);
uint32_t plat_ms(void);
uint32_t stub_sleep_total(void);

static int failures, checks;

/* out = a "/" b (no snprintf: gcc cannot see that the lengths fit) */
static void pjoin(char *out, size_t size, const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b);
	if (la + 1 + lb + 1 > size) {
		fprintf(stderr, "path too long\n");
		exit(2);
	}
	memcpy(out, a, la);
	out[la] = '/';
	memcpy(out + la + 1, b, lb + 1);
}

#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { long _a = (long)(a), _b = (long)(b); checks++; if (_a != _b) { failures++; \
	printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)

static char data_root[256], overlay[256];

static void put_file(const char *root, const char *rel, const char *content)
{
	char p[512];
	FILE *f;
	pjoin(p, sizeof p, root, rel);
	f = fopen(p, "wb");
	if (!f) {
		perror(p);
		exit(2);
	}
	fputs(content, f);
	fclose(f);
}

static void mk(const char *root, const char *rel)
{
	char p[512];
	pjoin(p, sizeof p, root, rel);
	mkdir(p, 0777);
}

/* a digest of a folder tree: names, sizes and contents */
static unsigned long tree_digest(const char *dir)
{
	unsigned long h = 5381;
	DIR *d = opendir(dir);
	struct dirent *e;
	char names[64][256];
	int n = 0, i, j;

	if (!d)
		return 0;
	while ((e = readdir(d)) && n < 64)
		if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
			strcpy(names[n++], e->d_name);
	closedir(d);
	for (i = 0; i < n; i++)
		for (j = i + 1; j < n; j++)
			if (strcmp(names[i], names[j]) > 0) {
				char t[256];
				strcpy(t, names[i]);
				strcpy(names[i], names[j]);
				strcpy(names[j], t);
			}
	for (i = 0; i < n; i++) {
		char p[512];
		struct stat st;
		const char *c;
		pjoin(p, sizeof p, dir, names[i]);
		for (c = names[i]; *c; c++)
			h = h * 33 + (unsigned char)*c;
		stat(p, &st);
		if (S_ISDIR(st.st_mode)) {
			h = h * 33 + tree_digest(p);
		} else {
			FILE *f = fopen(p, "rb");
			int ch;
			while (f && (ch = fgetc(f)) != EOF)
				h = h * 33 + (unsigned)ch;
			if (f)
				fclose(f);
		}
	}
	return h;
}

static int read_all(const char *dosname, char *buf, int max)
{
	int16_t h;
	uint16_t got;
	if (dosfs_open(dosname, 0, &h) != 0)
		return -1;
	dosfs_read(h, buf, (uint16_t)(max - 1), &got);
	buf[got] = 0;
	dosfs_close(h);
	return got;
}

static void test_dosfs(void)
{
	int16_t h, h2, hs[DOSFS_HANDLES];
	uint16_t n;
	uint32_t pos;
	char buf[256];
	unsigned long before;
	int i, k;

	strcpy(data_root, "/tmp/osiunit-data-XXXXXX");
	strcpy(overlay, "/tmp/osiunit-home-XXXXXX");
	if (!mkdtemp(data_root) || !mkdtemp(overlay)) {
		perror("mkdtemp");
		exit(2);
	}
	mk(data_root, "SAVEGAME");
	put_file(data_root, "SAVEGAME/OBJBLKAA", "data-aa");
	put_file(data_root, "SAVEGAME/OBJLIST", "objlist!");
	put_file(data_root, "u6pal", "palette");
	put_file(data_root, "MAPTILES.VGA", "tiles");
	mk(data_root, "Sub");
	mk(data_root, "Sub/Deep");
	put_file(data_root, "Sub/Deep/File.Txt", "deep");
	before = tree_digest(data_root);
	stub_set_dirs(overlay, data_root);
	CHECK_EQ(dosfs_init(data_root, overlay), 0);

	/* names: case, separators, drive, root, . and .., 8.3 cut */
	CHECK_EQ(read_all("savegame\\objblkaa", buf, sizeof buf), 7);
	CHECK(strcmp(buf, "data-aa") == 0);
	CHECK_EQ(read_all("SAVEGAME/OBJBLKAA", buf, sizeof buf), 7);
	CHECK_EQ(read_all("U6PAL", buf, sizeof buf), 7);
	CHECK(strcmp(buf, "palette") == 0);
	CHECK_EQ(read_all("c:\\u6pal", buf, sizeof buf), 7);
	CHECK_EQ(read_all("\\u6pal", buf, sizeof buf), 7);
	CHECK_EQ(read_all("savegame\\..\\u6pal", buf, sizeof buf), 7);
	CHECK_EQ(read_all(".\\u6pal", buf, sizeof buf), 7);
	CHECK_EQ(read_all("maptilesXYZ.vgaQ", buf, sizeof buf), 5);
	CHECK_EQ(read_all("sub\\deep\\file.txt", buf, sizeof buf), 4);
	CHECK(dosfs_canonical("savegame/objblkaa.tmp") && strcmp(dosfs_canonical("savegame/objblkaa.tmp"), "SAVEGAME\\OBJBLKAA.TMP") == 0);

	/* errors */
	CHECK_EQ(dosfs_open("nofile", 2, &h), DOSERR_NOFILE);
	CHECK_EQ(dosfs_open("nodir\\x", 2, &h), DOSERR_NOPATH);
	CHECK_EQ(dosfs_open("savegame", 2, &h), DOSERR_ACCESS);
	CHECK_EQ(dosfs_open("a:u6pal", 2, &h), DOSERR_NOPATH);
	CHECK_EQ(dosfs_open("*.vga", 2, &h), DOSERR_NOFILE);
	CHECK_EQ(dosfs_open("savegame\\\\objlist", 2, &h), DOSERR_NOPATH);
	CHECK_EQ(dosfs_close(99), DOSERR_HANDLE);
	CHECK_EQ(dosfs_close(-1), DOSERR_HANDLE);
	CHECK_EQ(dosfs_read(17, buf, 4, &n), DOSERR_HANDLE);
	CHECK_EQ(dosfs_create("nodir\\x", &h), DOSERR_NOPATH);
	CHECK_EQ(dosfs_create("savegame", &h), DOSERR_ACCESS);

	/* handles: lowest free from 5, 20 in all */
	CHECK_EQ(dosfs_open("u6pal", 0, &h), 0);
	CHECK_EQ(h, 5);
	CHECK_EQ(dosfs_open("u6pal", 0, &h2), 0);
	CHECK_EQ(h2, 6);
	dosfs_close(h);
	CHECK_EQ(dosfs_open("u6pal", 0, &h), 0);
	CHECK_EQ(h, 5);
	dosfs_close(h);
	dosfs_close(h2);
	for (i = 0; i < 15; i++) {
		CHECK_EQ(dosfs_open("u6pal", 0, &hs[i]), 0);
		CHECK_EQ(hs[i], 5 + i);
	}
	CHECK_EQ(dosfs_open("u6pal", 0, &h), DOSERR_TOOMANY);
	for (i = 0; i < 15; i++)
		dosfs_close(hs[i]);

	/* copy on write: the data root file is copied to the overlay on the first write */
	CHECK_EQ(dosfs_open("savegame\\objblkaa", 2, &h), 0);
	CHECK_EQ(dosfs_open("SAVEGAME\\OBJBLKAA", 0, &h2), 0);
	CHECK_EQ(dosfs_seek(h, 0, 2, &pos), 0);
	CHECK_EQ(pos, 2);
	CHECK_EQ(dosfs_write(h, "XYZ", 3, &n), 0);
	CHECK_EQ(n, 3);
	CHECK_EQ(dosfs_read(h2, buf, 20, &n), 0);   /* the other handle sees the write */
	buf[n] = 0;
	CHECK(strcmp(buf, "daXYZaa") == 0);
	dosfs_close(h);
	dosfs_close(h2);
	CHECK_EQ(read_all("savegame\\objblkaa", buf, sizeof buf), 7);
	CHECK(strcmp(buf, "daXYZaa") == 0);
	CHECK(dosfs_host_path("savegame\\objblkaa") && strstr(dosfs_host_path("savegame\\objblkaa"), overlay) != 0);
	CHECK(tree_digest(data_root) == before);
	/* mode 0 cannot write, mode 1 cannot read (DOS: access denied) */
	CHECK_EQ(dosfs_open("u6pal", 0, &h), 0);
	CHECK_EQ(dosfs_write(h, "x", 1, &n), DOSERR_ACCESS);
	dosfs_close(h);
	CHECK_EQ(dosfs_open("u6pal", 1, &h), 0);
	CHECK_EQ(dosfs_read(h, buf, 1, &n), DOSERR_ACCESS);
	dosfs_close(h);

	/* create, sizes, seek, zero-length write = set the size */
	CHECK_EQ(dosfs_create("savegame\\objblkzz.tmp", &h), 0);
	CHECK_EQ(dosfs_write(h, "0123456789", 10, &n), 0);
	CHECK_EQ(dosfs_seek(h, 2, 0, &pos), 0);
	CHECK_EQ(pos, 10);
	CHECK_EQ(dosfs_seek(h, 0, 4, &pos), 0);
	CHECK_EQ(dosfs_write(h, "", 0, &n), 0);
	CHECK_EQ(dosfs_seek(h, 2, 0, &pos), 0);
	CHECK_EQ(pos, 4);
	CHECK_EQ(dosfs_seek(h, 0, 12, &pos), 0);
	CHECK_EQ(dosfs_write(h, "", 0, &n), 0);
	CHECK_EQ(dosfs_seek(h, 2, 0, &pos), 0);
	CHECK_EQ(pos, 12);
	CHECK_EQ(dosfs_seek(h, 0, 0, &pos), 0);
	CHECK_EQ(dosfs_read(h, buf, 100, &n), 0);
	CHECK_EQ(n, 12);
	CHECK(memcmp(buf, "0123\0\0\0\0\0\0\0\0", 12) == 0);
	CHECK_EQ(dosfs_seek(h, 1, -2, &pos), 0);
	CHECK_EQ(pos, 10);
	CHECK_EQ(dosfs_read(h, buf, 100, &n), 0);   /* past the end: nothing */
	CHECK_EQ(n, 2);
	CHECK_EQ(dosfs_seek(h, 0, 100, &pos), 0);
	CHECK_EQ(dosfs_read(h, buf, 100, &n), 0);
	CHECK_EQ(n, 0);
	dosfs_close(h);
	CHECK(dosfs_exists("SAVEGAME\\OBJBLKZZ.TMP"));
	CHECK(tree_digest(data_root) == before);

	/* delete: a data root file is hidden from then on, also after a restart */
	CHECK_EQ(dosfs_delete("maptiles.vga"), 0);
	CHECK_EQ(dosfs_open("maptiles.vga", 0, &h), DOSERR_NOFILE);
	CHECK_EQ(dosfs_delete("maptiles.vga"), DOSERR_NOFILE);
	CHECK_EQ(dosfs_delete("nodir\\maptiles.vga"), DOSERR_NOPATH);
	CHECK_EQ(dosfs_delete("savegame"), DOSERR_ACCESS);
	CHECK_EQ(dosfs_init(data_root, overlay), 0);
	CHECK(!dosfs_exists("maptiles.vga"));
	CHECK_EQ(read_all("savegame\\objblkaa", buf, sizeof buf), 7);   /* the overlay copy */
	CHECK(strcmp(buf, "daXYZaa") == 0);
	CHECK_EQ(dosfs_create("maptiles.vga", &h), 0);   /* created again: visible */
	dosfs_close(h);
	CHECK(dosfs_exists("maptiles.vga"));
	CHECK_EQ(read_all("maptiles.vga", buf, sizeof buf), 0);
	/* a file in the overlay that shadows a data root file: deleting it hides both */
	CHECK_EQ(dosfs_delete("savegame\\objblkaa"), 0);
	CHECK(!dosfs_exists("savegame\\objblkaa"));
	CHECK(tree_digest(data_root) == before);

	/* rename: the object.c sequence (delete the save, rename the .tmp over it) */
	CHECK_EQ(dosfs_rename("savegame\\objblkzz.tmp", "savegame\\objlist"), DOSERR_ACCESS);
	CHECK_EQ(dosfs_rename("savegame\\nothere", "savegame\\x"), DOSERR_NOFILE);
	CHECK_EQ(dosfs_rename("savegame\\objblkzz.tmp", "nodir\\x"), DOSERR_NOPATH);
	CHECK_EQ(dosfs_delete("savegame\\objlist"), 0);
	CHECK_EQ(dosfs_rename("savegame\\objblkzz.tmp", "savegame\\objlist"), 0);
	CHECK_EQ(read_all("savegame\\objlist", buf, sizeof buf), 12);
	CHECK(!dosfs_exists("savegame\\objblkzz.tmp"));
	/* renaming a data root file copies it */
	CHECK_EQ(dosfs_rename("u6pal", "u6pal.bak"), 0);
	CHECK(!dosfs_exists("u6pal"));
	CHECK_EQ(read_all("U6PAL.BAK", buf, sizeof buf), 7);
	CHECK(strcmp(buf, "palette") == 0);
	CHECK_EQ(dosfs_init(data_root, overlay), 0);
	CHECK(!dosfs_exists("u6pal"));
	CHECK(dosfs_exists("u6pal.bak"));
	CHECK(dosfs_exists("savegame\\objlist"));
	/* the overlay keeps the data root's directory names */
	{
		char p[512];
		struct stat st;
		pjoin(p, sizeof p, overlay, "SAVEGAME/OBJLIST");
		CHECK(stat(p, &st) == 0);
		CHECK_EQ(dosfs_create("sub\\deep\\new.dat", &h), 0);
		dosfs_close(h);
		pjoin(p, sizeof p, overlay, "Sub/Deep/NEW.DAT");
		CHECK(stat(p, &st) == 0);
	}
	CHECK(tree_digest(data_root) == before);

	/* standard handles 0-4 are devices */
	CHECK_EQ(dosfs_write(1, "x", 1, &n), 0);
	CHECK_EQ(n, 1);
	CHECK_EQ(dosfs_read(0, buf, 1, &n), 0);
	CHECK_EQ(n, 0);
	(void)k;
}

/* ---- the OSI file protocol ---- */

static int hcalls;
static int16_t hcode[16], hfunc[16];
static char *hfile[16];
static int clear_times;   /* the handler clears OSI_ErrorCode (retry) this many times */
static int create_on_error;

static void handler(void)
{
	if (hcalls < 16) {
		hcode[hcalls] = OSI_ErrorCode;
		hfunc[hcalls] = OSI_ErrorFunc;
		hfile[hcalls] = OSI_ErrorFile;
	}
	hcalls++;
	if (create_on_error) {
		int16_t h;
		create_on_error = 0;
		dosfs_create(OSI_ErrorFile, &h);
		dosfs_close(h);
	}
	if (clear_times > 0) {
		clear_times--;
		OSI_ErrorCode = 0;
	}
}

static void reset_handler(int clears)
{
	hcalls = 0;
	clear_times = clears;
	create_on_error = 0;
	OSI_ErrorHandler.f = (void *(*)())(void (*)(void))handler;
}

static void test_osi_file(void)
{
	char name[] = "newfile";
	char missing[] = "missing";
	char big[] = "big.dat";
	char objlist[] = "savegame\\objlist";
	char bak[] = "u6pal.bak";
	static uint8_t buf[0x30000];
	int16_t h, fd;
	uint32_t r;
	int i;

	/* the default handler is OSI_callback */
	CHECK(OSI_ErrorHandler.f == (void *(*)())(void (*)(void))OSI_callback);
	h = OSI_open(missing);
	CHECK_EQ(h, -1);
	CHECK_EQ(OSI_ErrorCode, 2);
	CHECK_EQ(OSI_ErrorFunc, 1);
	CHECK(OSI_ErrorFile == missing);
	/* default handler: a failed read block counts AX = the error code (a bare RET) */
	r = OSI_read(13, 0xFFFFFFFFu, 100, buf);
	CHECK_EQ(r, 6);
	CHECK_EQ(OSI_ErrorCode, -1);
	CHECK_EQ(OSI_ErrorFunc, 5);

	reset_handler(0);
	h = OSI_open(missing);
	CHECK_EQ(h, -1);
	CHECK_EQ(hcalls, 1);
	CHECK_EQ(hcode[0], 2);
	CHECK_EQ(hfunc[0], 1);
	CHECK(hfile[0] == missing);
	/* retry: the handler makes the file and clears the code */
	reset_handler(1);
	create_on_error = 1;
	h = OSI_open(name);
	CHECK(h >= 5);
	CHECK_EQ(hcalls, 1);
	OSI_close(h);
	/* close of a bad handle, retried twice then given up */
	reset_handler(2);
	CHECK_EQ(OSI_close(42), -1);
	CHECK_EQ(hcalls, 3);
	CHECK_EQ(hcode[2], 6);
	CHECK_EQ(hfunc[2], 2);

	/* reads: blocks of FFF0h, short read reported as -1, position kept with offset -1 */
	reset_handler(0);
	fd = OSI_create(big);
	CHECK(fd >= 5);
	for (i = 0; i < 0x20000; i++)
		buf[i] = (uint8_t)(i * 13 + (i >> 8));
	r = OSI_write(fd, 0, 0x20000, buf);
	CHECK_EQ(r & 0xFFFF, 0x20000 - 2 * 0xFFF0);
	CHECK_EQ(hcalls, 0);
	CHECK_EQ(OSI_fileSize(fd), 0x20000);
	memset(buf, 0, sizeof buf);
	r = OSI_read(fd, 0, 0x18000, buf);
	CHECK_EQ(r, 0x18000);
	CHECK_EQ(hcalls, 0);
	CHECK(buf[0x17FFF] == (uint8_t)(0x17FFF * 13 + (0x17FFF >> 8)));
	r = OSI_read(fd, 0xFFFFFFFFu, 0x10, buf);   /* current position */
	CHECK_EQ(r, 0x10);
	CHECK(buf[0] == (uint8_t)(0x18000 * 13 + (0x18000 >> 8)));
	r = OSI_read(fd, 0x1FFF0, 0x100, buf);      /* short: reported */
	CHECK_EQ(r, 0x10);
	CHECK_EQ(hcalls, 1);
	CHECK_EQ(hcode[0], -1);
	CHECK_EQ(hfunc[0], 5);
	reset_handler(0);
	r = OSI_read(fd, 0x10000, 0x1FFF0, buf);    /* short in the first block */
	CHECK_EQ(r, 0x10000);
	CHECK_EQ(hcalls, 1);
	/* OSI_fileSize leaves the position at the end */
	reset_handler(0);
	OSI_fileSize(fd);
	r = OSI_read(fd, 0xFFFFFFFFu, 4, buf);
	CHECK_EQ(r, 0);
	CHECK_EQ(hcalls, 1);
	OSI_close(fd);
	/* a failed seek reports once and is not retried */
	reset_handler(5);
	r = OSI_read(77, 0, 10, buf);
	CHECK_EQ(r, 0);
	CHECK_EQ(hcalls, 1);
	CHECK_EQ(hcode[0], 6);
	/* a failed read block is retried while the handler clears the code */
	reset_handler(2);
	r = OSI_read(77, 0xFFFFFFFFu, 10, buf);
	CHECK_EQ(r, 0);
	CHECK_EQ(hcalls, 4);
	CHECK_EQ(hcode[0], 6);
	CHECK_EQ(hcode[2], 6);
	CHECK_EQ(hcode[3], -1);
	/* delete and rename */
	reset_handler(0);
	CHECK_EQ(OSI_delete(missing), -1);
	CHECK_EQ(hcode[0], 2);
	CHECK_EQ(hfunc[0], 4);
	reset_handler(0);
	CHECK_EQ(OSI_rename(bak, objlist), -1);
	CHECK_EQ(hcode[0], 5);
	CHECK_EQ(hfunc[0], 7);
	CHECK(hfile[0] == bak);
	CHECK_EQ(OSI_delete(big), 0);
	/* OSI_copy */
	reset_handler(0);
	CHECK_EQ(OSI_copy(bak, name), 0);
	{
		char b[64];
		CHECK_EQ(read_all(name, b, sizeof b), 7);
		CHECK(strcmp(b, "palette") == 0);
	}
	/* the pascal-case aliases */
	fd = OSI_open(bak);
	CHECK_EQ(OSI_Read(fd, 0, 3, buf), 3);
	OSI_close(fd);
	OSI_ErrorHandler.f = (void *(*)())(void (*)(void))OSI_callback;
}

/* ---- timer ---- */

static void test_timer(void)
{
	uint32_t t0;

	stub_set_fake_clock(1, 0);
	OSI_delay_init();
	CHECK_EQ(osi_ticks(), 0);
	OSI_delay(1);
	CHECK_EQ(osi_ticks(), 1);
	CHECK_EQ(plat_ms(), 55);            /* tick 1 starts at 54.9 ms */
	OSI_delay(0);
	CHECK_EQ(plat_ms(), 55);
	stub_set_fake_clock(1, 300);        /* 5 ticks pass while the game works */
	OSI_delay(2);                       /* already due: no wait */
	CHECK_EQ(plat_ms(), 300);
	OSI_delay(1);                       /* counted from the previous return */
	CHECK_EQ(osi_ticks(), 6);
	CHECK_EQ(plat_ms(), 330);           /* tick 6 = 329.6 ms */
	OSI_delay(30);
	CHECK_EQ(osi_ticks(), 36);
	t0 = plat_ms();
	CHECK(t0 >= 1977 && t0 <= 1978);
	/* 16-bit counter wrap: 65536 ticks later OSI_delay(1) waits again */
	stub_set_fake_clock(1, t0 + 3600000u);   /* about 65543 ticks */
	OSI_delay(10);
	CHECK((uint16_t)(osi_ticks() - 36) >= 10);
	/* after OSI_delay_clean the counter is frozen */
	OSI_delay_clean();
	OSI_delay(0);
	stub_set_fake_clock(0, 0);
}

/* ---- record and replay: the game sees the recorded ticks and keys, without waiting ---- */

static void scripted(int16_t *out, int n)
{
	int i;
	OSI_delay_init();
	for (i = 0; i < n; i++) {
		if (i % 3 == 0)
			stub_key_push((uint16_t)(0x1E61 + i));
		out[2 * i] = (int16_t)KBD_GetKey(i & 1);
		OSI_delay(1 + (i % 2));
		out[2 * i + 1] = (int16_t)osi_ticks();
	}
}

static void test_replay(void)
{
	char path[256];
	int16_t rec[80], rep[80];
	int st;
	pid_t pid;
	uint32_t ms0;

	pjoin(path, sizeof path, overlay, "RECORD.U6R");
	pid = fork();
	if (pid == 0) {
		FILE *f;
		stub_set_fake_clock(1, 1000);
		rp_start_record(path);
		scripted(rec, 40);
		rp_flush();
		pjoin(path, sizeof path, overlay, "expect.bin");
		f = fopen(path, "wb");
		fwrite(rec, sizeof rec, 1, f);
		fclose(f);
		_exit(0);
	}
	waitpid(pid, &st, 0);
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
	{
		FILE *f;
		char p2[256];
		pjoin(p2, sizeof p2, overlay, "expect.bin");
		f = fopen(p2, "rb");
		CHECK(f != 0);
		if (f) {
			CHECK_EQ(fread(rec, sizeof rec, 1, f), 1);
			fclose(f);
		}
	}
	CHECK_EQ(rp_start_replay(path), 0);
	stub_set_fake_clock(1, 777777);   /* another time, other live keys: not seen */
	stub_key_clear();
	stub_key_push(0x2C7A);
	ms0 = stub_sleep_total();
	scripted(rep, 40);
	CHECK(memcmp(rec, rep, sizeof rec) == 0);
	CHECK_EQ(stub_sleep_total(), ms0);   /* no sleeping under replay */
	/* the recording is over: the live clock takes over from the last replayed tick */
	{
		uint32_t last = (uint32_t)(uint16_t)rep[79], t;
		t = osi_ticks();
		CHECK(rp_finished());
		CHECK_EQ((uint16_t)t, (uint16_t)last);
		OSI_delay(2);
		t = osi_ticks();
		CHECK_EQ((uint16_t)t, (uint16_t)(last + 2));
		CHECK(stub_sleep_total() - ms0 <= 115);
	}
	rp_take_over();
	OSI_delay(1);
	CHECK(stub_sleep_total() - ms0 <= 175);
	stub_set_fake_clock(0, 0);
	stub_key_clear();
}

/* ---- dosmem, dosint ---- */

static int callback_calls;
static void drive_cb(void) { callback_calls++; }

static void test_mem_int(void)
{
	void *p;
	int x;
	union REGS r;
	struct SREGS s;
	uint8_t *pal;

	dosmem_init();
	CHECK_EQ(dos_farcoreleft(), DOS_HEAP_END - DOS_HEAP_START - 8);
	p = dos_farmalloc(dos_farcoreleft());
	CHECK(p != 0);
	CHECK_EQ(dos_fp_seg(p), DOS_HEAP_START >> 4);
	CHECK_EQ(dos_fp_off(p), 8);
	CHECK_EQ(dos_farcoreleft(), 0);
	CHECK(dos_farmalloc(16) == 0);
	CHECK_EQ(OSI_memory(), 0);
	dos_farfree(p);
	CHECK_EQ(dos_farcoreleft(), DOS_HEAP_END - DOS_HEAP_START - 8);
	CHECK(dos_mk_fp(0x1234, 0x10) == dos_base + 0x12350);
	CHECK_EQ(dos_fp_seg(dos_base + 0x12357), 0x1235);
	CHECK_EQ(dos_fp_off(dos_base + 0x12357), 7);
	CHECK(dos_fp_seg(&x) >= DOS_SEG_FOREIGN);
	CHECK(dos_mk_fp(dos_fp_seg(&x), dos_fp_off(&x)) == (void *)&x);
	CHECK(dos_mk_fp(dos_fp_seg(&x), (uint16_t)(dos_fp_off(&x) + 2)) == (void *)((char *)&x + 2));

	memset(&r, 0, sizeof r);
	r.h.ah = 0x30;
	intdos(&r, &r);
	CHECK_EQ(r.h.al, 5);
	CHECK_EQ(r.h.ah, 0);
	r.x.ax = 0x4408;
	r.h.bl = 3;
	intdos(&r, &r);
	CHECK_EQ(r.x.cflag, 0);
	CHECK_EQ(r.x.ax, 1);
	r.x.ax = 0x4408;
	r.h.bl = 1;
	intdos(&r, &r);
	CHECK(r.x.cflag != 0);
	r.x.ax = 0x440E;
	r.h.bl = 3;
	intdos(&r, &r);
	CHECK_EQ(r.x.cflag, 0);
	CHECK_EQ(r.h.al, 0);
	int86(0x11, &r, &r);
	CHECK_EQ(r.x.ax & 1, 0);
	CHECK_EQ(OSI_getDrive(), 'C');
	DriveErrorCallback = drive_cb;
	CHECK_EQ(OSI_selectDrive('c'), 1);
	CHECK_EQ(OSI_selectDrive('A'), 0);
	CHECK_EQ(OSI_selectDrive('#'), 0);
	CHECK_EQ(callback_calls, 2);
	CHECK_EQ(OSI_getDrive(), 'C');
	/* the VGA DAC */
	r.x.ax = 0x1010;
	r.x.bx = 0xFF;
	r.h.dh = 12;
	r.h.ch = 13;
	r.h.cl = 14;
	int86(0x10, &r, &r);
	CHECK(vga_palette[765] == 12 && vga_palette[766] == 13 && vga_palette[767] == 14);
	pal = (uint8_t *)dos_mk_fp(0x2000, 0);
	pal[0] = 1;
	pal[1] = 2;
	pal[2] = 3;
	pal[3] = 4;
	pal[4] = 5;
	pal[5] = 6;
	segread(&s);
	s.es = 0x2000;
	r.x.ax = 0x1012;
	r.x.bx = 0x10;
	r.x.cx = 2;
	r.x.dx = 0;
	int86x(0x10, &r, &r, &s);
	CHECK(vga_palette[48] == 1 && vga_palette[53] == 6);
	outportb(0x3C8, 0xE0);
	outportb(0x3C9, 7);
	outportb(0x3C9, 8);
	outportb(0x3C9, 9);
	CHECK(vga_palette[0xE0 * 3] == 7 && vga_palette[0xE0 * 3 + 2] == 9);
	/* execl fails as a missing program */
	stub_set_fake_clock(1, 0);
	errno = 0;
	CHECK_EQ(execl("end.exe", "end.exe", "vmn", "1", "2", "3", 0), -1);
	CHECK_EQ(errno, ENOENT);
	CHECK_EQ(execl("ultima6.exe", "ultima6.exe", "vmn", 0), -1);
	stub_set_fake_clock(0, 0);
	/* the KBD translation */
	CHECK_EQ(osi_kbd_translate(0x1E61), 'a');
	CHECK_EQ(osi_kbd_translate(0x4800), 0x148);
	CHECK_EQ(osi_kbd_translate(0x4838), 0x148);
	CHECK_EQ(osi_kbd_translate(0x4A2D), '-');
	CHECK_EQ(osi_kbd_translate(0x0E08), 0x10E);
	CHECK_EQ(osi_kbd_translate(0x0000), 0);
}

int main(void)
{
	stub_quiet_log(1);
	test_dosfs();
	test_osi_file();
	test_timer();
	test_replay();
	test_mem_int();
	printf("test_osi_unit: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
