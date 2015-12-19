/* term.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_TERM

#include "term.h"

#include <stdio.h>
#include <string.h>

#include <libsrsbsns/ringbuf.h>

#include "common.h"
#include "intlog.h"

struct termtype {
	char name[32];
	term_attach_fn attach_fn;
};

#define TERMREG(NAME) void NAME ## _attach(struct term_if *ifc);
#include "terms.h"
#undef TERMREG

static struct termtype s_termtype[] = {
	#define TERMREG(NAME) { #NAME, NAME ## _attach },
	#include "terms.h"
	#undef TERMREG
};

static struct term_if s_termif;

static ringbuf_t s_input;


bool
term_attach(const char *termname)
{
	for (size_t i = 0; i < COUNTOF(s_termtype); i++) {
		if (strcmp(s_termtype[i].name, termname) == 0) {
			s_termtype[i].attach_fn(&s_termif);
			return true;
		}
	}

	E("could not attach `%s`", termname);
	return false;
}

void
term_input(uint8_t *data, size_t len)
{
	if (!s_input)
		s_input = ringbuf_init(4096);
	
	ringbuf_put(s_input, data, len);

	while (!ringbuf_empty(s_input)) {
		uint8_t buf[256];
		size_t n = ringbuf_peek(s_input, buf, sizeof buf);

		ssize_t eaten = s_termif.input_fn(buf, n);
		if (eaten == -1)
			C("term input function failed");

		if (eaten == 0)
			break; //"not enough data"

		if (ringbuf_get(s_input, buf, eaten) != (size_t)eaten)
			C("ringbuf_get failed...");
	}
}

void
term_dumpnames(void)
{
	for (size_t i = 0; i < COUNTOF(s_termtype); i++)
		puts(s_termtype[i].name);
}
