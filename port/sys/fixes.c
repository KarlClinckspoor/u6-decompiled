/* The fixes (fixes.h, docs/port/FIXES.md). */
#include "fixes.h"
#include <stdlib.h>
#include <string.h>

uint32_t enhance_on;

static const struct {
	const char *name;
	const char *cr;
	const char *about;
} tab[FIX_COUNT] = {
	{"vortex-cube", "CR-001", "the vortex cube counts only moonstones (eight other items no longer end the game)"},
	{"inventory-scroll", "CR-002", "scrolling an open bag's inventory after readying an item no longer hangs the game"},
	{"cast-target", "CR-003", "casting at a map target uses up the spell's own reagent count, not a byte of memory"},
	{"keg-fuse", "CR-004", "a powder keg's fuse goes out with the keg (no longer blows up what takes its place)"},
	{"healer-body", "CR-005", "a healer raises a body the party member carries, and only a person's"},
	{"lens-beam", "CR-006", "the lens beams are drawn at the edge of the view, never outside it"},
	{"full-object-table", "CR-007", "objects that do not fit when an area loads stay in its file (no longer lost)"},
	{"fountain-wish", "CR-008", "a fountain grants only the five wishes it lists (an empty wish no longer matches)"},
	{"map-seam", "CR-010", "objects keep their order where the map wraps around (a ship at each edge)"},
};

static int index_of(const char *name, size_t n)
{
	int i;
	for (i = 0; i < FIX_COUNT; i++)
		if (strlen(tab[i].name) == n && !strncmp(tab[i].name, name, n))
			return i;
	for (i = 0; i < FIX_COUNT; i++)     /* or its finding: cr-004, CR-004 */
		if (n == 6 && !strncmp(tab[i].cr + 3, name + 3, 3) && (name[0] | 32) == 'c' && (name[1] | 32) == 'r' && name[2] == '-')
			return i;
	return -1;
}

int fixes_parse(const char *list, uint32_t *mask)
{
	uint32_t m = *mask;
	const char *p = list;
	while (p && *p) {
		const char *e = strchr(p, ',');
		size_t n = e ? (size_t)(e - p) : strlen(p);
		int off = 0, i;
		while (n && *p == ' ')
			p++, n--;
		while (n && p[n - 1] == ' ')
			n--;
		if (n && *p == '-')
			off = 1, p++, n--;
		if (n == 3 && !strncmp(p, "all", 3))
			m = off ? 0 : (1u << FIX_COUNT) - 1;
		else if ((n == 4 && !strncmp(p, "none", 4)) || (n == 1 && *p == '0'))
			m = 0;
		else if (n == 1 && *p == '1')
			m = (1u << FIX_COUNT) - 1;
		else if (n) {
			if ((i = index_of(p, n)) < 0) {
				fprintf(stderr, "u6: no fix %.*s (--fixes list)\n", (int)n, p);
				return -1;
			}
			m = off ? m & ~(1u << i) : m | (1u << i);
		}
		p = e ? e + 1 : 0;
	}
	*mask = m;
	return 0;
}

char *enhance_names(uint32_t mask, char *buf, int n)
{
	int i, k = 0;
	if (n > 0)
		buf[0] = 0;
	for (i = 0; i < FIX_COUNT; i++)
		if ((mask >> i) & 1u && k < n)
			k += snprintf(buf + k, (size_t)(n - k), "%s%s", k ? "," : "", tab[i].name);
	return buf;
}

void fixes_list(FILE *f)
{
	int i;
	fprintf(f, "Fixes of the original game's faults, all off unless turned on (--fixes all, or\n"
		"fixes=all in u6port.cfg); off, the port plays exactly as DOS (docs/port/FIXES.md):\n\n");
	for (i = 0; i < FIX_COUNT; i++)
		fprintf(f, "  %-18s %s  %s\n", tab[i].name, tab[i].cr, tab[i].about);
}

int fixes_from_recording(const char *path, uint32_t *mask)
{
	unsigned char h[15];
	char names[512];
	unsigned len;
	FILE *f = fopen(path, "rb");
	*mask = 0;
	if (!f)
		return 0;                   /* the replay itself says it cannot open it */
	if (fread(h, 1, 12, f) != 12 || (h[4] != 5 && h[4] != 6)) {
		fclose(f);
		return 0;                   /* format 4: DOS's, or the port's without fixes */
	}
	if (fread(h + 12, 1, 3, f) != 3 || h[12] != 9 || (len = h[13] | (unsigned)h[14] << 8) >= sizeof names
	    || fread(names, 1, len, f) != len) {
		fclose(f);
		fprintf(stderr, "u6: %s is a format %d recording without its fixes\n", path, h[4]);
		return -1;
	}
	fclose(f);
	names[len] = 0;
	if (fixes_parse(names, mask) < 0) {
		fprintf(stderr, "u6: %s was recorded with a fix this build does not have (%s)\n", path, names);
		return -1;
	}
	return 0;
}

void fixes_log(void)
{
	char b[512];
	if (enhance_on)
		fprintf(stderr, "u6: fixes: %s (not as DOS: gameplay changed)\n", enhance_names(enhance_on, b, sizeof b));
}
