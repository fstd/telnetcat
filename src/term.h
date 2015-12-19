/* term.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_TERM_H
#define TELNETCAT_TERM_H 1


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>


struct term_if;

typedef ssize_t (*term_input_fn)(uint8_t *data, size_t len);
typedef void (*term_attach_fn)(struct term_if *ifc);

struct term_if {
	term_input_fn input_fn;
};


bool term_attach(const char *termname);
void term_input(uint8_t *data, size_t len);

void term_dumpnames(void);

#endif /* TELNETCAT_TERM_H */
