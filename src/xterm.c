/* xterm.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_XTERM

#include "xterm.h"

#include "term.h"
#include "common.h"
#include "intlog.h"
#include "screen.h"

ssize_t
xterm_input(uint8_t *data, size_t len)
{
	C("xterm not implemented");
}

void
xterm_attach(struct term_if *ifc)
{
	ifc->input_fn = xterm_input;
	C("xterm not implemented");
}
