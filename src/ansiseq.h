/* ansiseq.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_ANSISEQ_H
#define TELNETCAT_ANSISEQ_H 1


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


#define MAX_ANSISEQ_PARAMS 8
#define MAX_OSC_STR_SZ 256
#define ABSENT INT_MIN


struct ansiseq {
	uint8_t cls;
	uint8_t cmd;
	uint8_t strarg[MAX_OSC_STR_SZ];
	bool ext;
	int argv[MAX_ANSISEQ_PARAMS];
	int argc;
};

typedef void (*ansiseq_cb_fn)(struct ansiseq *cs);

int ansiseq_parsectl(uint8_t *data, size_t len, struct ansiseq *dst);

void ansiseq_register(uint8_t cls, uint8_t cmd, ansiseq_cb_fn fn);

void ansiseq_dump(struct ansiseq *cs);

#endif /* TELNETCAT_ANSISEQ_H */
