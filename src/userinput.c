/* userinput.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_UINPUW

#include "userinput.h"

#include <stdio.h>
#include <stdlib.h>

#include <libsrsbsns/bufrd.h>

#include <err.h>

#include "intlog.h"
#include "common.h"

static int s_fd = -1;
static bufrd_t s_bufrd;

bool
userinput_init(int fd)
{
	s_fd = fd;
	return (s_bufrd = bufrd_init(fd, 4096));
}

bool
userinput_haschar(void)
{
	return bufrd_buffered(s_bufrd);
}

int
userinput_getchar(void)
{
	return bufrd_getchar(s_bufrd);
}

int
userinput_fd(void)
{
	return s_fd;
}
