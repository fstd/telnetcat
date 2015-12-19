/* nullterm.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_NULLTERM

#include "nullterm.h"

#include "term.h"
#include "intlog.h"

ssize_t
nullterm_input(uint8_t *data, size_t len)
{
	I("input");
	return len;
}

void
nullterm_attach(struct term_if *ifc)
{
	ifc->input_fn = nullterm_input;
	N("attached");
}
