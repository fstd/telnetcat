/* common.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_COMMON_H
#define TELNETCAT_COMMON_H 1


#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>


#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define STRTOSZ(STR) (size_t)strtoull((STR), NULL, 0)
#define STRTOU(STR) (unsigned)strtoul((STR), NULL, 0)
#define COUNTOF(ARR) (sizeof (ARR) / sizeof (ARR)[0])


void *xmalloc(size_t sz);
uint64_t millisecs(void);
bool write_all(int fd, const uint8_t *data, size_t len);


#endif /* TELNETCAT_COMMON_H */
