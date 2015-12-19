/* userinput.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_USERINPUT_H
#define TELNETCAT_USERINPUT_H 1


#include <stdbool.h>


bool userinput_init(int fd);
bool userinput_haschar(void);
int userinput_getchar(void);
int userinput_fd(void);


#endif /* TELNETCAT_USERINPUT_H */
