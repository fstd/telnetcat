/* telnet.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_TELNET

#include "telnet.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sys/socket.h>

#include <libsrsbsns/addr.h>
#include <libsrsbsns/bufrd.h>

#include <err.h>

#include "intlog.h"
#include "common.h"


#define DEF_BUFSZ 4096


/* telnet control actions ('act') */
#define DONT 0xfe
#define WONT 0xfc
#define WILL 0xfb
#define DO 0xfd
#define SUBOPT_START 0xfa
#define SUBOPT_END 0xf0

/* telnet option types ('what') */
#define AUTH 0x25
#define ENCRYPTION 0x26
#define SUPPRESS_GA 0x03
#define TERM_TYPE 0x18
#define WIND_SIZE 0x1f
#define TERM_SPEED 0x20
#define REM_FLOW_CTL 0x21
#define LINEMODE 0x22
#define NEW_ENV_OPT 0x27
#define STATUS 0x05
#define X_DISPLAY 0x23
#define ECHO 0x01



static bool    s_online;
static int     s_sockfd = -1;
static bufrd_t s_sockreader;
static size_t  s_termwidth;
static size_t  s_termheight;
static char   *s_termtype;


static void write_ctl(int sck, uint8_t act, uint8_t what);
static void write_ctl_subopt(int sck, uint8_t what, const void *extra, size_t len);
static void write_logon(int sck) ;
static void handle_ctl(void);
static void srv_wont(int what);
static void srv_dont(int what);
static void srv_will(int what);
static void srv_do(int what);
static void srv_subopt(int what);
static int getchar_noeof(void);
static const char *actnam(int act);
static const char * whatnam(int what);


bool
telnet_connect(const char *host, uint16_t port,
    size_t w, size_t h, const char *term)
{
	struct sockaddr_storage sa;
	size_t asz;

	I("Connecting to %s:%"PRIu16" as a %zux%zu %s",
	    host, port, w, h, term);

	int sck = addr_connect_socket_p(host, port,
	    (struct sockaddr *)&sa, &asz, 10000000, 60000000);
	
	if (sck < 0)
		CE("could not connect socket");
	
	D("socket connected (%d)", sck);

	write_logon(sck);

	free(s_termtype);
	if (s_sockreader)
		bufrd_dispose(s_sockreader);
	
	s_sockfd = sck;
	s_sockreader = bufrd_init(sck, DEF_BUFSZ);
	s_online = true;
	s_termwidth = w, s_termheight = h;
	s_termtype = strdup(term);

	N("connected");

	return true;
}

bool
telnet_online(void)
{
	return s_online;
}

int
telnet_fd(void)
{
	return s_sockfd;
}

bool
telnet_haschar(void)
{
	return bufrd_buffered(s_sockreader);
}

int
telnet_getchar(void)
{
	if (!s_online) {
		E("not online");
		return -1;
	}

	int r = bufrd_getchar(s_sockreader);
	if (r == -1) {
		E("bufrd_getchar failed, disconnecting");
		s_online = false;
		close(s_sockfd);
		return -1;
	} else if ((unsigned)r == 0xffu) {
		handle_ctl();
		return -2;
	}

	V("telnet_getchar: 0x%02x (%c)", r, r);

	return r;
}

bool
telnet_write(const void *data, size_t len)
{
	if (s_sockfd == -1)
		return false;
	
	V("writing %zu bytes to server", len);
	
	return write_all(s_sockfd, data, len);
}

/* ---------------------------------------------------------------------- */

static void
write_ctl(int sck, uint8_t act, uint8_t what)
{
	uint8_t buf[] = {0xffu, act, what};
	D("to srv: %s %s (0x%02x 0x%02x)",
	    actnam(act), what ? whatnam(what) : "", act, what);

	write_all(sck, buf, sizeof buf - !what);
		
}

static void
write_ctl_subopt(int sck, uint8_t what, const void *extra, size_t len)
{
	write_ctl(sck, SUBOPT_START, what);

	if (len)
		write_all(sck, extra, len);

	write_ctl(sck, SUBOPT_END, 0);
}

static void
write_logon(int sck)
{
	V("Sending initial control sequences");
	write_ctl(sck, DO, SUPPRESS_GA);
	write_ctl(sck, WILL, WIND_SIZE);
	write_ctl(sck, WILL, TERM_TYPE);
	write_ctl(sck, DO, STATUS);
}


static void
handle_ctl(void)
{
	int act = getchar_noeof();
	int what = getchar_noeof();

	D("from srv: %s %s (0x%02x 0x%02x)", actnam(act), whatnam(what), act, what);

	switch (act) {
	case WILL:
		srv_will(what);
		break;
	case DO:
		srv_do(what);
		break;
	case DONT:
		srv_dont(what);
		break;
	case WONT:
		srv_wont(what);
		break;
	case SUBOPT_START:
		srv_subopt(what);
		break;
	}
}

static void
srv_wont(int what)
{
}

static void
srv_dont(int what)
{
}

static void
srv_will(int what)
{
	switch (what) {
	case ECHO:
		write_ctl(s_sockfd, DO, ECHO);
	}
}

static void
srv_do(int what)
{
	switch (what) {
	case WIND_SIZE:{
		uint16_t buf[2];
		buf[0] = htons(s_termwidth);
		buf[1] = htons(s_termheight);
		write_ctl_subopt(s_sockfd, what, buf, sizeof buf);
		break;
	}
	case TERM_TYPE: {
		char term[32];
		snprintf(term+1, sizeof term - 1, "%s", s_termtype);
		term[0] = '\0';
		char *t = term + 1;
		while (*t) {
			*t = toupper((int)*t);
			t++;
		}
		write_ctl_subopt(s_sockfd, what, term, 5);
		break;
	}
	default:
		write_ctl(s_sockfd, WONT, what);
	}

}

static void
srv_subopt(int what)
{
	int last = 0, cur = 0;
	while (last != 0xff || cur != SUBOPT_END) {
		last = cur;
		cur = getchar_noeof();
	}
	/* we don't care for now */
	D("from srv: %s %s (0x%02x 0x%02x)",
	    actnam(SUBOPT_END), whatnam(what), SUBOPT_END, what);
}

static int
getchar_noeof(void)
{
	int r = bufrd_getchar(s_sockreader);
	if (r == EOF)
		C("unexpected EOF or error from server");
	return r;
}

/* debugging */
static const char *
actnam(int act)
{
	return act == WILL         ? "WILL" :
	       act == WONT         ? "WON'T" :
	       act == DONT         ? "DON'T" :
	       act == DO           ? "DO" :
	       act == SUBOPT_START ? "SO_START" :
	       act == SUBOPT_END   ? "SO_SEND" : "UNKNOWN";
}

static const char *
whatnam(int what)
{
	return what == 0x25 ? "AUTH " :
	       what == 0x26 ? "ENCRYPTION " :
	       what == 0x03 ? "SUPPRESS_GA " :
	       what == 0x18 ? "TERM_TYPE " :
	       what == 0x1f ? "WIND_SIZE " :
	       what == 0x20 ? "TERM_SPEED " :
	       what == 0x21 ? "REM_FLOW_CTL " :
	       what == 0x22 ? "LINEMODE " :
	       what == 0x27 ? "NEW_ENV_OPT " :
	       what == 0x05 ? "STATUS " :
	       what == 0x23 ? "X_DISPLAY " :
	       what == 0x01 ? "ECHO " : "UNKNOWN";
}

