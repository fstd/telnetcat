/* common.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "common.h"

#include <stdio.h>

#include <unistd.h>
#include <sys/time.h>

#include "intlog.h"

void *
xmalloc(size_t sz)
{
	void *r = malloc(sz);
	if (!r)
		CE("malloc");
	return r;
}

uint64_t
millisecs(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) != 0)
		CE("gettimeofday failed");
	
	return tv.tv_sec * 1000ull + tv.tv_usec / 1000u;
}

bool
write_all(int fd, const uint8_t *data, size_t len)
{
	size_t bc = 0;

	while (bc < len) {
		ssize_t r = write(fd, data + bc, len - bc);
		if (r <= 0)
			CE("write failed (%zd)", r);

		bc += r;
	}

	return true;
}

char
hexchar(int d)
{
	if (d < 0 || d >= 16)
		C("out of range: %d", d);

	return d + (d < 10 ? '0' : 'A' - 10);
}
