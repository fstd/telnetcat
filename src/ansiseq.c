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
#include <stdio.h>

#include "intlog.h"
#include "common.h"

#define ST_ST 0 /* Starting state */
#define ST_E1 1 /* escape char seen */
#define ST_CSI 2 /* CSI state */
#define ST_PAC 3 /* ) state */
#define ST_PAO 4 /* ( state */
#define ST_SQC 5 /* ] state */
#define ST_ERR 6 /* error state (accepting) */
#define ST_FIN 7 /* okay state (accepting) */
#define ST_IGN 8 /* ignore state (accepting) */

#define TK_ESC   0 /* Escape character */
#define TK_SQO 1 /* [ */
#define TK_SQC 2 /* ] */
#define TK_PAC 3 /* ) */
#define TK_PAO 4 /* ( */
#define TK_QM 5 /* ? */
#define TK_SC 6 /* ; */
#define TK_ALPHA 7 /* a-zA-Z */
#define TK_DIGIT 8 /* 0-9 */
#define TK_CH    9 /* Anything else */

struct transition {
	int state;
	void (*action)(int);
};

static int s_parmv = 0;
static struct ansiseq s_curseq;
static struct ansiseq s_protoseq;
static bool s_gotparm;
static bool s_valid;

static bool s_init = false;

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
	s_valid = false;
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
	s_valid = true;
}

void
act_mkpar(int c)
{
	s_parmv = s_parmv * 10 + (c - '0');
	s_gotparm = true;
}

void
act_ext(int c)
{
	s_curseq.ext = false;
}

int
mktok(int c)
{
	if (isdigit(c))
		return TK_DIGIT;

	if (isalpha(c))
		return TK_ALPHA;

	switch (c) {
	case '\033': return TK_ESC;
	case '[':    return TK_SQO;
	case ']':    return TK_SQC;
	case ')':    return TK_PAC;
	case '(':    return TK_PAO;
	case '?':    return TK_QM;
	case ';':    return TK_SC;
	default:     return TK_CH;
	}
}

struct transition delta[9][10] = {
/*          TK_ESC              TK_SQO             TK_SQC                TK_PAC               TK_PAO              */
/*            TK_QM               TK_SC               TK_ALPHA             TK_DIGIT             TK_CH             */
/*ST_ST */ {{ST_E1, act_begin}, {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop}},

/*ST_E1 */ {{ST_ERR, act_nop},  {ST_CSI, act_nop},  {ST_SQC, act_nop},   {ST_PAC, act_nop},   {ST_PAO, act_nop},
              {ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_IGN, act_nop},   {ST_IGN, act_nop}},

/*ST_CSI*/ {{ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_CSI, act_ext},  {ST_CSI, act_stor}, {ST_FIN, act_mkctl}, {ST_CSI, act_mkpar}, {ST_FIN, act_mkctl}},

/*ST_PAC*/ {{ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_IGN, act_nop},   {ST_ERR, act_nop}},

/*ST_PAO*/ {{ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_IGN, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop}},

/*ST_SQC*/ {{ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_ERR, act_nop},  {ST_IGN, act_nop},  {ST_ERR, act_nop},   {ST_SQC, act_nop},   {ST_ERR, act_nop}},

/*ST_ERR*/ {{ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop}},

/*ST_FIN*/ {{ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop}},

/*ST_IGN*/ {{ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop},
              {ST_ERR, act_nop},  {ST_ERR, act_nop},  {ST_ERR, act_nop},   {ST_ERR, act_nop},   {ST_ERR, act_nop}},
};

int
ansiseq_parsectl(uint8_t *data, size_t len, struct ansiseq *dst)
{
	size_t i = 0;

	int st = ST_ST;
	struct transition tr;
	int c;

	if (!s_init)
		ansiseq_init();
	
	while (i < len) {
		c = data[i++];
		tr = delta[st][mktok(c)];
		tr.action(c);
		st = tr.state;

		if (st == ST_ERR)
			C("error state reached through %d", c);

		if (st == ST_IGN || st == ST_FIN)
			break;
	}

	if (st != ST_IGN && st != ST_FIN && st != ST_ERR)
		return -1; //not enough data

	if (st == ST_FIN)
		*dst = s_curseq;
	else
		*dst = s_protoseq;

	return i;
}

/* ---------------------------------------------------------------------- */

static void
ansiseq_init(void)
{
	for (size_t i = 0; i < MAX_ANSISEQ_PARAMS; i++)
		s_protoseq.argv[i] = ABSENT;
	s_init = true;
}

