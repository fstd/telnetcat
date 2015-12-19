/* telnetcat.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_CORE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include <getopt.h>
#include <err.h>
#include <unistd.h>

#include <libsrsbsns/addr.h>
#include <libsrsbsns/io.h>

#include "intlog.h"
#include "common.h"
#include "screen.h"
#include "userinput.h"
#include "telnet.h"
#include "term.h"


#define DEF_WIDTH 80
#define DEF_HEIGHT 24
#define DEF_IDLETIME 200u //ms
#define DEF_PORT 23
#define DEF_TERMTYPE "vt100"


static size_t      s_width = DEF_WIDTH;
static size_t      s_height = DEF_HEIGHT;
static unsigned    s_idletime = DEF_IDLETIME;
static char        s_host[256];
static uint16_t    s_port = DEF_PORT;
static const char *s_termtype = DEF_TERMTYPE;


static void process_args(int *argc, char ***argv);
static void init(int *argc, char ***argv);
static void usage(FILE *str, const char *a0, int ec);
static void update_logger(int verb, int fancy);


static void
init(int *argc, char ***argv)
{
	tnc_log_init();
	process_args(argc, argv);

	screen_init(s_width, s_height);

	if (!userinput_init(fileno(stdin)))
		C("could not init userinput");

	if (!term_attach(s_termtype))
		C("could not attach terminal");
	
	I("Initialized.  host: `%s`, port: %"PRIu16", term: `%s` (%zu x %zu), idletime: %u",
	    s_host, s_port, s_termtype, s_width, s_height, s_idletime);
}

static void
process_args(int *argc, char ***argv)
{
	char *a0 = (*argv)[0];

	for(int ch; (ch = getopt(*argc, *argv, "W:H:t:i:lcvqh")) != -1;) {
		switch (ch) {
		case 'W':
			if (!(s_width = STRTOSZ(optarg)))
				s_width = DEF_WIDTH;
			break;
		case 'H':
			if (!(s_height = STRTOSZ(optarg)))
				s_height = DEF_HEIGHT;
			break;
		case 'l':
			term_dumpnames();
			exit(0);
		case 'c':
			update_logger(0, 1);
			break;
		case 'v':
			update_logger(1, -1);
			break;
		case 'q':
			update_logger(-1, -1);
			break;
		case 't':
			s_termtype = strdup(optarg);
			break;
		case 'i':
			s_idletime = STRTOU(optarg);
			break;
		case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, a0, EXIT_FAILURE);
		}
	}

	*argc -= optind;
	*argv += optind;

	if (!*argc)
		C("Need an argument (hostname[:port])");
	
	addr_parse_hostspec_p(s_host, sizeof s_host, &s_port, (*argv)[0]);
	if (!s_port)
		s_port = DEF_PORT;
}

static void
usage(FILE *str, const char *a0, int ec)
{
	#define U(STR) fputs(STR "\n", str)
	U("======================");
	U("== telnetcat v"PACKAGE_VERSION" ==");
	U("======================");
	fprintf(str, "usage 1: %s -l\n", a0);
	fprintf(str, "usage 2: %s [-vqc] [-t <term>] [-i <idlems>]\n", a0);
	fprintf(str, "  [-W <width>] [-H <height>] <server>[:<port>]\n");
	U("");
	U("\t-t <str>: Set terminal type to <str>");
	U("\t-i <num>: Output screen if unchanged for <num> milliseconds");
	U("\t-W <num>: Terminal width in chars");
	U("\t-H <num>: Terminal height in chars");
	U("");
	U("\t-l: List supported terminal types and exit");
	U("\t-v: Be more verbose on stderr");
	U("\t-q: Be less verbose on stderr");
	U("\t-c: Use ANSI color sequences on stderr");
	U("\t-h: Display brief usage statement and terminate");
	U("");
	U("We establish a telnet connection to <server>, pretending to be");
	U("and rendering the received data to an off-screen terminal, which");
	U("we then dump to stdout whenever it is unchanged for the period");
	U("given by -i (and has not been printed yet)");
	U("");
	U("(C) 2015, Timo Buhrmester (contact: #fstd on irc.freenode.org)");
	#undef U
	exit(ec);
}

static void
update_logger(int verb, int fancy)
{
	int v = tnc_log_getlvl(MOD_CORE) + verb;
	if (v < 0)
		v = 0;

	if (fancy == -1)
		fancy = tnc_log_getfancy();

	tnc_log_setfancy(fancy);
	tnc_log_setlvl_all(v);
}


int
main(int argc, char **argv)
{
	init(&argc, &argv);

	if (!telnet_connect(s_host, s_port, s_width, s_height, s_termtype))
		C("could not telnet_connect()");
	
	int tfd = telnet_fd();
	int ufd = userinput_fd();
	
	uint64_t lastdata = 0;
	bool printed = false;

	for (;;) {
		bool tcanread = telnet_haschar();
		bool ucanread = userinput_haschar();

		if (!tcanread && !ucanread) {
			int64_t timeout;
			if (printed)
				timeout = 10000;
			else {
				/* figure out the correct timeout that brings
				 * us close to the time we'd output possible
				 * changes to the screen */
				timeout = (int64_t)lastdata + s_idletime - millisecs();
				if (timeout < 0)
					timeout = 10;
			}

			int r = io_select2r(&tcanread, &ucanread, tfd, ufd, timeout * 1000ull, true);
			if (r < 0)
				C("select failed");

			/* if we haven't heard from the server in s_idletime ms,
			 * and if we haven't printed the last screen updates
			 * yet, and if the screen actually changed since
			 * we last printed it, output it. */
			if (!tcanread && !printed && millisecs() >= lastdata + s_idletime) {
				if (screen_changed())
					screen_output();

				printed = true;
			}

			/* nothing selected */
			if (r == 0)
				continue;
		}

		uint8_t buf[512];
		size_t n;

		/* read data from the user, relay to server */
		if (ucanread) {
			n = 0;
			do {
				int ch = userinput_getchar();
				if (ch == -1)
					C("userinput_getchar failed");

				buf[n++] = ch;
			} while (n < sizeof buf && userinput_haschar());

			if (!telnet_write(buf, n))
				C("telnet_write failed");
		}

		/* read data from the server, feed to terminal */
		if (tcanread) {
			n = 0;
			do {
				int ch = telnet_getchar();
				if (ch == -1)
					C("telnet_getchar failed");

				if (ch == -2)
					continue; //handled a telnet ctl

				buf[n++] = ch;
			} while (n < sizeof buf && telnet_haschar());

			if (n > 0) {
				term_input(buf, n);
				lastdata = millisecs();
				printed = false;
			}
		}
	}

	return EXIT_SUCCESS;
}
