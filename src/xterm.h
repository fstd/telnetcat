/* xterm.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_XTERM_H
#define TELNETCAT_XTERM_H 1


#include "term.h"


ssize_t xterm_input(uint8_t *data, size_t len);

void xterm_attach(struct term_if *ifc);


#endif /* TELNETCAT_XTERM_H */
