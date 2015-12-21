/* screen.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_SCREEN_H
#define TELNETCAT_SCREEN_H 1


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>


#define ATTR_BOLD 0
#define ATTR_INV 1
#define ATTR_ULINE 2
#define ATTR_BLINK 3

#define COL_BLACK 0
#define COL_RED 1
#define COL_GREEN 2
#define COL_YELLOW 3
#define COL_BLUE 4
#define COL_PINK 5
#define COL_TEAL 6
#define COL_GRAY 7

#define DEF_FGCOL COL_GRAY
#define DEF_BGCOL COL_BLACK


void screen_init(size_t width, size_t height);

int screen_width(void);
int screen_height(void);
void screen_resize(size_t width, size_t height);

void screen_setattr(int attr, int fgcol, int bgcol);
void screen_resetattr(void);
void screen_getattr(uint8_t *attr, uint8_t *fgcol, uint8_t *bgcol);

size_t screen_curx(void);
size_t screen_cury(void);

void screen_setchar(char c);
void screen_putchar(char c);
void screen_puts(const char *str);
void screen_goto(ssize_t x, ssize_t y);
void screen_go(ssize_t x, ssize_t y);
void screen_fill_rect(size_t x1, size_t y1, size_t x2, size_t y2, uint8_t ch);

void screen_output(bool human);

bool screen_changed(void);


#endif /* TELNETCAT_SCREEN_H */
