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
static void handle_ansiseq(struct ansiseq *cs);

static void termop_curup(bool ext, int argc, int *argv);
static void termop_curdown(bool ext, int argc, int *argv);
static void termop_curleft(bool ext, int argc, int *argv);
static void termop_curright(bool ext, int argc, int *argv);
static void termop_setcursor(bool ext, int argc, int *argv);
static void termop_clear(bool ext, int argc, int *argv);
static void termop_showcursor(bool ext, int argc, int *argv);
static void termop_hidecursor(bool ext, int argc, int *argv);
static void termop_set_cursor_row(bool ext, int argc, int *argv);
static void termop_eraseinline(bool ext, int argc, int *argv);
static void termop_set_cursor_column(bool ext, int argc, int *argv);
static void termop_setcolor(bool ext, int argc, int *argv);

typedef void (*termop_fn)(bool ext, int argc, int *argv);

static termop_fn termop_dispatch[256] = {
	['m'] = termop_setcolor,
	['H'] = termop_setcursor,
	['h'] = termop_showcursor,
	['l'] = termop_hidecursor,
	['J'] = termop_clear,
	['r'] = termop_hidecursor,
	['d'] = termop_set_cursor_row,
	['K'] = termop_eraseinline,
	['G'] = termop_set_cursor_column,
	['A'] = termop_curup,
	['B'] = termop_curdown,
	['C'] = termop_curright,
	['D'] = termop_curleft,
};

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
			if (cs.cmd)
				handle_ansiseq(&cs);
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
handle_ansiseq(struct ansiseq *cs)
{
	if (!termop_dispatch[cs->cmd])
		C("no handler for ansi sequence %d `%c`", cs->cmd, cs->cmd);
	
	termop_dispatch[cs->cmd](cs->ext, cs->argc, cs->argv);

}

static void
termop_curup(bool ext, int argc, int *argv)
{
	screen_go(0, -1);
}

static void
termop_curdown(bool ext, int argc, int *argv)
{
	screen_go(0, 1);
}

static void
termop_curleft(bool ext, int argc, int *argv)
{
	screen_go(-1, 0);
}

static void
termop_curright(bool ext, int argc, int *argv)
{
	screen_go(1, 0);
}


static void
termop_setcursor(bool ext, int argc, int *argv)
{
	size_t y = argv[0] == ABSENT ? 0 : argv[0] - 1;
	size_t x = argv[1] == ABSENT ? 0 : argv[1] - 1;
	screen_goto(x, y);
}

static void
termop_clear(bool ext, int argc, int *argv)
{
	int mode = argv[0] == ABSENT ? 0 : argv[0];
	ssize_t tmp;

	switch (mode) {
	case 0:
		screen_fill_rect(0, screen_cury()+1, screen_width()-1, screen_height()-1, ' ');
		screen_fill_rect(screen_curx(), screen_cury(), screen_width()-1, screen_cury(), ' ');
		break;
	case 1:
		tmp = screen_cury() - 1;
		if (tmp < 0)
			tmp = 0;
		screen_fill_rect(0, 0, screen_width()-1, tmp, ' ');
		screen_fill_rect(0, screen_cury(), screen_curx(), screen_cury(), ' ');
		break;
	case 2:
		screen_fill_rect(0, 0, screen_width()-1, screen_height()-1, ' ');
		break;
	default:
		C("unimplemented clear mode: %d", mode);
	}
}

static void
termop_showcursor(bool ext, int argc, int *argv)
{
}

static void
termop_hidecursor(bool ext, int argc, int *argv)
{
}

static void
termop_set_cursor_row(bool ext, int argc, int *argv)
{
	screen_goto(-1, argv[0] == ABSENT ? 0 : argv[0] - 1);
}

static void
termop_eraseinline(bool ext, int argc, int *argv)
{
	int mode = argv[0] == ABSENT ? 0 : argv[0];

	if (mode < 0 || mode > 2)
		C("unexpected mode %d", mode);

	size_t start = mode > 0 ? 0u : screen_curx();
	size_t end = mode != 1 ? (size_t)(screen_width() - 1) : screen_curx();

	screen_fill_rect(start, screen_cury(), end, screen_cury(), ' ');
}

static void
termop_set_cursor_column(bool ext, int argc, int *argv)
{
	screen_goto(argv[0] == ABSENT ? 0 : argv[0] - 1, -1);
}

static void
termop_setcolor(bool ext, int argc, int *argv)
{
	if (argc == 0) {
		screen_resetattr();
		return;	
	}

	for (int i = 0; i < argc; i++) {
		if (argv[i] >= 30 && argv[i] <= 37) {
			screen_setattr(-1, argv[i] - 30, -1);
		} else if (argv[i] >= 40 && argv[i] <= 47) {
			screen_setattr(-1, -1, argv[i] - 40);
		} else {
			uint8_t curattr, curfgcol, curbgcol;
			screen_getattr(&curattr, &curfgcol, &curbgcol);
			switch (argv[i]) {
			case 0:
				curattr = 0;
				curfgcol = DEF_FGCOL;
				curbgcol = DEF_BGCOL;
				break;
			case 1:
				curattr |= 1 << ATTR_BOLD;
				break;
			case 7:
				curattr |= 1 << ATTR_INV;
				break;
			case 27:
				curattr &= ~(1 << ATTR_INV);
				break;
			case 39:
				curfgcol = DEF_FGCOL;
				break;
			case 49:
				curbgcol = DEF_BGCOL;
				break;
			default:
				C("unsupported m code %d", argv[i]);
			}

			screen_setattr(curattr, curfgcol, curbgcol);
		}
	}
}
