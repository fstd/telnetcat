/* ansiseq.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_ANSISEQ_H
#define TELNETCAT_ANSISEQ_H 1


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


#define MAX_ANSISEQ_PARAMS 8
#define ABSENT INT_MIN


struct ansiseq {
	bool ext;
	int argv[MAX_ANSISEQ_PARAMS];
	int argc;
	uint8_t cmd;
};


int ansiseq_parsectl(uint8_t *data, size_t len, struct ansiseq *dst);


#endif /* TELNETCAT_ANSISEQ_H */
