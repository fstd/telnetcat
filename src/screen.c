/* screen.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_SCREEN

#include "screen.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <err.h>

#include "intlog.h"
#include "common.h"

struct cellattr {
	uint8_t attr;
	uint8_t fgcol;
	uint8_t bgcol;
};


struct screen {
	size_t height, width;
	uint8_t **canvas;
	struct cellattr **attr;
};


static struct screen *s_scr;

static struct cellattr s_attr_cur;
static struct cellattr s_attr_normal =
    { .attr = 0, .fgcol = DEF_FGCOL, .bgcol = DEF_BGCOL };

static size_t s_curx, s_cury;
static bool s_changed;


static void clamp_cursor(void);
static void adv_cursor(void);
static struct screen *alloc_screen(size_t width, size_t height);
static void dealloc_screen(struct screen *s);
static void copy_screen(struct screen *dst, struct screen *src);
static void fill_rect(struct screen *s,
    size_t x1, size_t y1, size_t x2, size_t y2, uint8_t ch);


void 
screen_init(size_t width, size_t height)
{
	s_scr = alloc_screen(width, height);
	s_attr_cur = s_attr_normal;
	screen_fill_rect(0, 0, width-1, height-1, ' ');
	s_changed = true;
}


int 
screen_width(void)
{
	return s_scr->width;
}

int 
screen_height(void)
{
	return s_scr->height;
}

void 
screen_resize(size_t width, size_t height)
{
	struct screen *nscr = alloc_screen(width, height);
	fill_rect(nscr, 0, 0, width-1, height-1, ' ');
	copy_screen(nscr, s_scr);
	dealloc_screen(s_scr);
	s_scr = nscr;
	s_changed = true;
}

void
screen_setattr(int attr, int fgcol, int bgcol)
{
	if (attr >= 0)
		s_attr_cur.attr = attr;
	if (fgcol >= 0)
		s_attr_cur.fgcol = fgcol;
	if (bgcol >= 0)
		s_attr_cur.bgcol = bgcol;
}

void
screen_resetattr(void)
{
	s_attr_cur = s_attr_normal;
}

void
screen_getattr(uint8_t *attr, uint8_t *fgcol, uint8_t *bgcol)
{
	if (attr)
		*attr = s_attr_cur.attr;
	if (fgcol)
		*fgcol = s_attr_cur.fgcol;
	if (bgcol)
		*bgcol = s_attr_cur.bgcol;
}

size_t
screen_curx(void)
{
	return s_curx;
}

size_t
screen_cury(void)
{
	return s_cury;
}

void
screen_setchar(char c)
{
	uint8_t oc = s_scr->canvas[s_cury][s_curx];
	s_scr->canvas[s_cury][s_curx] = c;
	s_scr->attr[s_cury][s_curx] = s_attr_cur;
	if ((uint8_t)c != oc)
		s_changed = true;
}

void
screen_putchar(char c)
{
	screen_setchar(c);
	adv_cursor();
}

void
screen_puts(const char *str)
{
	while (*str)
		screen_putchar(*str++);
}

void
screen_goto(ssize_t x, ssize_t y)
{
	if (x >= 0)
		s_curx = x;
	if (y >= 0)
		s_cury = y;

	clamp_cursor();
}

void
screen_go(ssize_t x, ssize_t y)
{
	ssize_t ncx = (ssize_t)s_curx + x;
	ssize_t ncy = (ssize_t)s_cury + y;
	if (ncx < 0) ncx = 0;
	if (ncy < 0) ncy = 0;

	s_curx = (size_t)ncx;
	s_cury = (size_t)ncy;
	
	clamp_cursor();
}

void
screen_fill_rect(size_t x1, size_t y1, size_t x2, size_t y2, uint8_t ch)
{
	fill_rect(s_scr, x1, y1, x2, y2, ch);
}

void
screen_output(bool human)
{
	printf("I %"PRIu64" %zu %zu %zu %zu\n",
	    millisecs(), s_scr->width, s_scr->height, s_curx, s_cury);

	for (size_t y = 0; y < s_scr->height; y++) {
		printf("L\t%zu\t", y);
		for (size_t x = 0; x < s_scr->width; x++)
			if (human)
				putchar(s_scr->canvas[y][x]);
			else
				printf("%c%c%c%c",
				    hexchar(s_scr->attr[y][x].attr),
				    hexchar(s_scr->attr[y][x].fgcol),
				    hexchar(s_scr->attr[y][x].bgcol),
				    s_scr->canvas[y][x]);
		puts("");
	}
}

bool
screen_changed(void)
{
	bool r = s_changed;
	s_changed = false;
	return r;
}

/* ---------------------------------------------------------------------- */

static void
clamp_cursor(void)
{
	if (s_curx >= s_scr->width)
		s_curx = s_scr->width - 1;

	if (s_cury >= s_scr->height)
		s_cury = s_scr->height - 1;
}

static void
adv_cursor(void)
{
	s_curx++;
	if (s_curx >= s_scr->width) {
		s_curx = 0;
		if (s_cury+1 < s_scr->height)
			s_cury++;
	}
}

static struct screen *
alloc_screen(size_t width, size_t height)
{
	uint8_t **canvas = xmalloc(height * sizeof *canvas);
	for (size_t i = 0; i < height; i++) {
		canvas[i] = xmalloc(width+1);
		canvas[i][width] = '\0';
	}

	struct cellattr **attr = xmalloc(height * sizeof *attr);
	for (size_t i = 0; i < height; i++) {
		attr[i] = xmalloc((width+1) * sizeof *attr[i]);
	}

	struct screen *scr = xmalloc(sizeof *scr);
	scr->canvas = canvas, scr->height = height, scr->width = width, scr->attr = attr;

	return scr;
}

static void
dealloc_screen(struct screen *s)
{
	for (size_t i = 0; i < s->height; i++)
		free(s->canvas[i]);
	
	free(s->canvas);
	free(s);
}

static void
copy_screen(struct screen *dst, struct screen *src)
{
	size_t width = MIN(src->width, dst->width);
	size_t height = MIN(src->height, dst->height);

	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			dst->canvas[y][x] = src->canvas[y][x];
			dst->attr[y][x] = src->attr[y][x];
		}
	}
}

static void
fill_rect(struct screen *s, size_t x1, size_t y1, size_t x2, size_t y2,
    uint8_t ch)
{
	size_t tmp;
	if (x1 > x2)
		tmp = x1, x1 = x2, x2 = tmp;
	if (y1 > y2)
		tmp = y1, y1 = y2, y2 = tmp;
	
	if (x1 >= s->width) x1 = s->width - 1;
	if (x2 >= s->width) x2 = s->width - 1;
	if (y1 >= s->width) y1 = s->height - 1;
	if (y2 >= s->width) y2 = s->height - 1;
	
	for (size_t y = y1; y <= y2; y++) {
		memset(s->canvas[y]+x1, ch, x2-x1+1);
		for (size_t x = x1; x <= x2; x++)
			s->attr[y][x] = s_attr_cur;
	}
}
