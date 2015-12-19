/* vt100.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_VT100_H
#define TELNETCAT_VT100_H 1


#include "term.h"


ssize_t vt100_input(uint8_t *data, size_t len);

void vt100_attach(struct term_if *ifc);


#endif /* TELNETCAT_VT100_H */
