/* nullterm.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_NULLTERM_H
#define TELNETCAT_NULLTERM_H 1


#include "term.h"

ssize_t nullterm_input(uint8_t *data, size_t len);

void nullterm_attach(struct term_if *ifc);


#endif /* TELNETCAT_NULLTERM_H */
