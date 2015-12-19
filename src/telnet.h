/* telnet.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_TELNET_H
#define TELNETCAT_TELNET_H 1


#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


bool telnet_connect(const char *host, uint16_t port,
    size_t w, size_t h, const char *term);

bool telnet_online(void);
int telnet_fd(void);

bool telnet_haschar(void);
int telnet_getchar(void);
bool telnet_write(const void *data, size_t len);


#endif /* TELNETCAT_TELNET_H */
