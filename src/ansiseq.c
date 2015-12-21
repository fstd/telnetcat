/* ansiseq.c - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_ANSISEQ

#include "ansiseq.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "intlog.h"
#include "common.h"

#define S_STA 0 /* Starting state */
#define S_ESC 1 /* escape char seen */
#define S_CSI 2 /* CSI state */
#define S_SQC 3 /* ] state */
#define S_PPH 4 /* ()# state */
#define S_FIN 5 /* okay state (accepting) */
#define S_ERR 6 /* error state (accepting) */
#define NUM_STATES 7

/* digits can be command chars, too, but not for CSI */
#define CCHRS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz<=>"
#define T_ESCP 0 /* esc ('\033') */
#define T_CCHR 1 /* see CCHRS */
#define T_NDIG 2 /* '0' - '9' */
#define T_BELL 3 /* '\a' */
#define T_STRM 4 /* '\234' */
#define T_HASH 5 /* '#' */
#define T_OPRN 6 /* '(' */
#define T_CPRN 7 /* ')' */
#define T_OSQB 8 /* '[' */
#define T_CSQB 9 /* ']' */
#define T_QMRK 10 /* '?' */
#define T_SMCL 11 /* ';' */
#define T_REST 12 /* anything else */
#define NUM_TOKENS 13

#define MAX_CALLBACKS 64

struct transition {
	int state;
	void (*action)(int);
};

static bool s_init = false;
static struct ansiseq s_protoseq;

static struct ansiseq s_curseq;

static int s_parmv = 0;
static bool s_gotparm;

struct ansiseq_match {
	uint8_t cls;
	uint8_t cmd;
	ansiseq_cb_fn fn;
};

static struct ansiseq_match s_cbs[MAX_CALLBACKS];
static ansiseq_cb_fn s_cbunhandled;


static void ansiseq_init(void);


void
act_nop(int c)
{
}

void
act_begin(int c)
{
	s_curseq = s_protoseq;
	s_gotparm = false;
	s_parmv = 0;
}

void
act_stor(int c)
{
	if (c == ';')
		s_curseq.argv[s_curseq.argc++] = s_gotparm ? s_parmv : ABSENT;
	else if (s_gotparm)
		s_curseq.argv[s_curseq.argc++] = s_parmv;
	
	s_parmv = 0;
	s_gotparm = false;
}

void
act_mkctl(int c)
{
	if (s_gotparm)
		act_stor(0);
	
	s_curseq.cmd = c;
}

void
act_mkpar(int c)
{
	s_parmv = s_parmv * 10 + (c - '0');
	s_gotparm = true;
}

void
act_mkstr(int c)
{
	size_t len = strlen((const char *)s_curseq.strarg);
	if (len + 1 >= sizeof s_curseq.strarg) {
		W("OSC control sequence truncated");
		return;
	}

	s_curseq.strarg[len] = c;
}

void
act_setcls(int c)
{
	s_curseq.cls = c;
}

void
act_ext(int c)
{
	s_curseq.ext = true;
}

int
mktok(int c)
{
	if (c < 0 || c > 255)
		C("character out of range: %d", c);

	if (c == 0)           return T_REST;
	if (isdigit(c))       return T_NDIG;
	if (strchr(CCHRS, c)) return T_CCHR;

	switch (c) {
	case '\033': return T_ESCP;
	case '\007': return T_BELL;
	case '\234': return T_STRM;
	case '#':    return T_HASH;
	case '(':    return T_OPRN;
	case ')':    return T_CPRN;
	case '[':    return T_OSQB;
	case ']':    return T_CSQB;
	case '?':    return T_QMRK;
	case ';':    return T_SMCL;
	default:     return T_REST;
	}
}

#define ERR {S_ERR, act_nop}
struct transition delta[][NUM_TOKENS] = {
/*           T_ESCP              T_CCHR              T_NDIG              T_BELL            T_STRM            T_HASH               T_OPRN               T_CPRN               T_OSQ                T_CSQB               T_QMRK              T_SMCL              T_REST */
/* S_STA */ {{S_ESC, act_begin}, ERR,                ERR,                ERR,              ERR,              ERR,                 ERR,                 ERR,                 ERR,                 ERR,                 ERR,                ERR,                ERR,                },
/* S_ESC */ {{S_ESC, act_begin}, {S_FIN, act_mkctl}, {S_FIN, act_mkctl}, ERR,              ERR,              {S_PPH, act_setcls}, {S_PPH, act_setcls}, {S_PPH, act_setcls}, {S_CSI, act_setcls}, {S_SQC, act_setcls}, ERR,                ERR,                ERR,                },
/* S_CSI */ {{S_ESC, act_begin}, {S_FIN, act_mkctl}, {S_CSI, act_mkpar}, ERR,              ERR,              ERR,                 ERR,                 ERR,                 ERR,                 ERR,                 {S_CSI, act_ext},   {S_CSI, act_stor},  ERR,                },
/* S_SQC */ {{S_ESC, act_begin}, {S_SQC, act_mkstr}, {S_SQC, act_mkstr}, {S_FIN, act_nop}, {S_FIN, act_nop}, {S_SQC, act_mkstr},  {S_SQC, act_mkstr},  {S_SQC, act_mkstr},  {S_SQC, act_mkstr},  {S_SQC, act_mkstr},  {S_SQC, act_mkstr}, {S_SQC, act_mkstr}, {S_SQC, act_mkstr}, },
/* S_PPH */ {{S_ESC, act_begin}, {S_FIN, act_mkctl}, {S_FIN, act_mkctl}, ERR,              ERR,              ERR,                 ERR,                 ERR,                 ERR,                 ERR,                 ERR,                ERR,                ERR,                },
};
#undef ERR

int
ansiseq_parsectl(uint8_t *data, size_t len, struct ansiseq *dst)
{
	size_t i = 0;

	int st = S_STA;
	struct transition tr;
	int c;

	if (!s_init)
		ansiseq_init();
	
	while (i < len) {
		c = data[i++];
		tr = delta[st][mktok(c)];
		tr.action(c);
		st = tr.state;

		if (st == S_ERR)
			C("error state reached through %d", c);

		if (st == S_FIN)
			break;
	}

	if (st != S_FIN)
		return -1; //not enough data

	if (dst)
		*dst = s_curseq;

	bool handled = false;
	for(size_t j = 0; j < COUNTOF(s_cbs); j++)
		if (s_cbs[j].fn && s_cbs[j].cmd == s_curseq.cmd
		    && s_cbs[j].cls == s_curseq.cls)
			s_cbs[j].fn(&s_curseq), handled = true;

	if (!handled && s_cbunhandled)
		s_cbunhandled(&s_curseq);

	return i;
}

void
ansiseq_register(uint8_t cls, uint8_t cmd, ansiseq_cb_fn fn)
{
	size_t i = 0;
	while (i < COUNTOF(s_cbs) && s_cbs[i].fn)
		i++;
	
	if (i == COUNTOF(s_cbs))
		C("callback array full. bump MAX_CALLBACKS");

	if (!cls && !cmd)
		s_cbunhandled = fn;
	else {
		s_cbs[i].cmd = cmd;
		s_cbs[i].cls = cls;
		s_cbs[i].fn = fn;
	}
}


void
ansiseq_dump(struct ansiseq *cs)
{
	A("ansiseq class 0x%02x (%c), cmd 0x%02x (%c), ext: %d",
	    cs->cls, cs->cls?cs->cls:'-', cs->cmd, cs->cmd?cs->cmd:'-', cs->ext);
	A("strarg: '%s', argc: %d", cs->strarg, cs->argc);
	for (int i = 0; i < cs->argc; i++) {
		char x[64] = "(absent)";
		if (cs->argv[i] != ABSENT)
			snprintf(x, sizeof x, "%d", cs->argv[i]);
		A("argv[%d]: '%s'", i, x);
	}
}

/* ---------------------------------------------------------------------- */

static void
ansiseq_init(void)
{
	for (size_t i = 0; i < MAX_ANSISEQ_PARAMS; i++)
		s_protoseq.argv[i] = ABSENT;
	s_init = true;
}

