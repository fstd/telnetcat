/* vt100.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_VT100

#include "vt100.h"

#include "term.h"
#include "common.h"
#include "intlog.h"
#include "screen.h"
#include "ansiseq.h"


static void handle_inputchar(uint8_t ch);

static void termop_unhandled(struct ansiseq *s);
static void termop_charset(struct ansiseq *s);
static void termop_cleartab(struct ansiseq *s);
static void termop_curdown(struct ansiseq *s);
static void termop_curleft(struct ansiseq *s);
static void termop_curright(struct ansiseq *s);
static void termop_curup(struct ansiseq *s);
static void termop_dblwidth(struct ansiseq *s);
static void termop_eraseindisplay(struct ansiseq *s);
static void termop_eraseinline(struct ansiseq *s);
static void termop_index(struct ansiseq *s);
static void termop_indexrev(struct ansiseq *s);
static void termop_leds(struct ansiseq *s);
static void termop_newline(struct ansiseq *s);
static void termop_report(struct ansiseq *s);
static void termop_reset(struct ansiseq *s);
static void termop_resetmode(struct ansiseq *s);
static void termop_restcurattr(struct ansiseq *s);
static void termop_savecurattr(struct ansiseq *s);
static void termop_scrollregion(struct ansiseq *s);
static void termop_setattr(struct ansiseq *s);
static void termop_setcursor(struct ansiseq *s);
static void termop_setmode(struct ansiseq *s);
static void termop_settab(struct ansiseq *s);
static void termop_testfill(struct ansiseq *s);
static void termop_vt52(struct ansiseq *s);
static void termop_whatareyou(struct ansiseq *s);

ssize_t
vt100_input(uint8_t *data, size_t len)
{
	size_t bc = 0;
	while (bc < len) {
		if (data[bc] == '\033') {
			struct ansiseq cs;
			int r = ansiseq_parsectl(data+bc, len-bc, &cs);
			if (r == -1) { //not enough data
				//W("not enough data");
				return bc;
			}

			bc += r;
		} else
			handle_inputchar(data[bc++]);
	}

	return bc;
}

void
vt100_attach(struct term_if *ifc)
{
	ifc->input_fn = vt100_input;

	/* save cursor + attr */
	ansiseq_register(0, '7', termop_savecurattr);

	/* restore cursor + attr */
	ansiseq_register(0, '8', termop_restcurattr);

	/* index */
	ansiseq_register(0, 'D', termop_index);

	/* reverse index */
	ansiseq_register(0, 'M', termop_indexrev);

	/* new line */
	ansiseq_register(0, 'E', termop_newline);

	/* set tab at current column */
	ansiseq_register(0, 'H', termop_settab);

	/* reset + POST */
	ansiseq_register(0, 'c', termop_reset);

	/* CSI sequences */
	ansiseq_register('[', 'J', termop_eraseindisplay);
	ansiseq_register('[', 'K', termop_eraseinline);
	ansiseq_register('[', 'c', termop_whatareyou);
	ansiseq_register('[', 'g', termop_cleartab);
	ansiseq_register('[', 'n', termop_report);
	ansiseq_register('[', 'H', termop_setcursor);
	ansiseq_register('[', 'f', termop_setcursor);
	ansiseq_register('[', 'A', termop_curup);
	ansiseq_register('[', 'B', termop_curdown);
	ansiseq_register('[', 'C', termop_curright);
	ansiseq_register('[', 'D', termop_curleft);
	ansiseq_register('[', 'm', termop_setattr);
	ansiseq_register('[', 'r', termop_scrollregion);
	ansiseq_register('[', 'h', termop_setmode);
	ansiseq_register('[', 'l', termop_resetmode);

	/* the below sequences are all unsupported, but registered
	 * anyway to provide appropriate warning messages */

	/* double-width/height related */
	ansiseq_register('#', '3', termop_dblwidth);
	ansiseq_register('#', '4', termop_dblwidth);
	ansiseq_register('#', '5', termop_dblwidth);
	ansiseq_register('#', '6', termop_dblwidth);

	/* testing: fill screen with E */
	ansiseq_register('#', '8', termop_testfill);

	/* character set related, B is usascii */
	ansiseq_register('(', '0', termop_charset);
	ansiseq_register('(', '1', termop_charset);
	ansiseq_register('(', '2', termop_charset);
	ansiseq_register('(', 'A', termop_charset);
	ansiseq_register('(', 'B', termop_charset);

	/* character set related, B is usascii */
	ansiseq_register(')', '0', termop_charset);
	ansiseq_register(')', '1', termop_charset);
	ansiseq_register(')', '2', termop_charset);
	ansiseq_register(')', 'A', termop_charset);
	ansiseq_register(')', 'B', termop_charset);

	/* vt52 compat, also D, H (conflict) */
	ansiseq_register(0, '<', termop_vt52);
	ansiseq_register(0, '=', termop_vt52);
	ansiseq_register(0, '>', termop_vt52);
	ansiseq_register(0, 'A', termop_vt52);
	ansiseq_register(0, 'B', termop_vt52);
	ansiseq_register(0, 'C', termop_vt52);
	ansiseq_register(0, 'I', termop_vt52);
	ansiseq_register(0, 'J', termop_vt52);
	ansiseq_register(0, 'K', termop_vt52);
	ansiseq_register(0, 'F', termop_vt52);
	ansiseq_register(0, 'G', termop_vt52);

	/* leds */
	ansiseq_register('[', 'q', termop_leds);

	/* stuff we didn't even register */
	ansiseq_register(0, 0, termop_unhandled);
	N("attached");
}

/* ---------------------------------------------------------------------- */

static void
handle_inputchar(uint8_t ch)
{
	if (ch <= 0x1f) {
		switch (ch) {
		case '\b':
			screen_go(-1, 0);
			break;
		case '\r':
			screen_goto(0, -1);
			break;
		case '\n':
			screen_go(0, 1);
			break;
		case '\0':
		case '\a':
		case 14:
		case 15:
			break;
		default:
			C("control char %d seeni, wat do?", ch);
		}

		return;
	}
	
	screen_putchar(ch);
}

static void
termop_unhandled(struct ansiseq *s)
{
	E("unrecognized:");
	ansiseq_dump(s);
}

static void
termop_charset(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_dblwidth(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_vt52(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_leds(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_testfill(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}


static void
termop_curup(struct ansiseq *s)
{
	screen_go(0, -1);
}

static void
termop_curdown(struct ansiseq *s)
{
	screen_go(0, 1);
}

static void
termop_curleft(struct ansiseq *s)
{
	screen_go(-1, 0);
}

static void
termop_curright(struct ansiseq *s)
{
	screen_go(1, 0);
}


static void
termop_eraseindisplay(struct ansiseq *s)
{
	int mode = s->argv[0] == ABSENT ? 0 : s->argv[0];
	ssize_t tmp;

	switch (mode) {
	case 0:
		screen_fill_rect(0, screen_cury()+1,
		    screen_width()-1, screen_height()-1, ' ');

		screen_fill_rect(screen_curx(), screen_cury(),
		    screen_width()-1, screen_cury(), ' ');

		break;
	case 1:
		tmp = screen_cury() - 1;
		if (tmp < 0)
			tmp = 0;

		screen_fill_rect(0, 0, screen_width()-1, tmp, ' ');
		screen_fill_rect(0, screen_cury(),
		    screen_curx(), screen_cury(), ' ');

		break;
	case 2:
		screen_fill_rect(0, 0,
		    screen_width()-1, screen_height()-1, ' ');

		break;
	default:
		C("unimplemented clear mode: %d", mode);
	}
}

static void
termop_eraseinline(struct ansiseq *s)
{
	int mode = s->argv[0] == ABSENT ? 0 : s->argv[0];

	if (mode < 0 || mode > 2)
		C("unexpected mode %d", mode);

	size_t start = mode > 0 ? 0u : screen_curx();
	size_t end = mode != 1 ? (size_t)(screen_width() - 1) : screen_curx();

	screen_fill_rect(start, screen_cury(), end, screen_cury(), ' ');
}


static void
termop_index(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_indexrev(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}


static void
termop_savecurattr(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_restcurattr(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}


static void
termop_setattr(struct ansiseq *s)
{
	if (s->argc == 0) {
		screen_resetattr();
		return;	
	}

	for (int i = 0; i < s->argc; i++) {
		uint8_t curattr, curfgcol, curbgcol;

		screen_getattr(&curattr, &curfgcol, &curbgcol);

		switch (s->argv[i]) {
		case 0:
			curattr = 0;
			break;
		case 1:
			curattr |= 1 << ATTR_BOLD;
			break;
		case 4:
			curattr |= 1 << ATTR_ULINE;
			break;
		case 5:
			curattr |= 1 << ATTR_BLINK;
			break;
		case 7:
			curattr |= 1 << ATTR_INV;
			break;
		default:
			W("unsupported m code %d", s->argv[i]);
		}

		screen_setattr(curattr, curfgcol, curbgcol);
	}
}

static void
termop_setcursor(struct ansiseq *s)
{
	size_t y = (!s->argv[0] || s->argv[0] == ABSENT) ? 0 : s->argv[0] - 1;
	size_t x = (!s->argv[1] || s->argv[1] == ABSENT) ? 0 : s->argv[1] - 1;
	screen_goto(x, y);
}


static void
termop_settab(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_cleartab(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}


static void
termop_newline(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_report(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_reset(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_setmode(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_resetmode(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_scrollregion(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}

static void
termop_whatareyou(struct ansiseq *s)
{
	W("unhandled");
	ansiseq_dump(s);
}
