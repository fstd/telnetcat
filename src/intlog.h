/* intlog.h - (C) 2015, Timo Buhrmester
 * telnetcat - telnet scrape base
 * See README for contact-, COPYING for license information. */

#ifndef TELNETCAT_INTLOG_H
#define TELNETCAT_INTLOG_H 1


#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>


#define MOD_CORE 0
#define MOD_COMMON 1
#define MOD_TELNET 2
#define MOD_UINPUT 3
#define MOD_SCREEN 4
#define MOD_TERM 5
#define MOD_VT100 6
#define MOD_XTERM 7
#define MOD_NULLTERM 8
#define MOD_ANSISEQ 9
#define MOD_UNKNOWN 10
#define NUM_MODS 11 /* when adding modules, don't forget intlog.c's `modnames' */

/* our two higher-than-debug custom loglevels */
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_VIVI 8
#define LOG_TRACE 9

#ifndef LOG_MODULE
# define LOG_MODULE MOD_UNKNOWN
#endif

//[TVDINWE](): log with Trace, Vivi, Debug, Info, Notice, Warn, Error severity.
//[TVDINWE]E(): similar, but also append ``errno'' message
//C(), CE(): as above, but also call exit(EXIT_FAILURE)

// ----- logging interface -----

#define V(...)                                                                 \
 tnc_log_log(LOG_MODULE,LOG_VIVI,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define VE(...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_VIVI,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define D( ...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_DEBUG,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define DE(...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_DEBUG,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define I(...)                                                                 \
 tnc_log_log(LOG_MODULE,LOG_INFO,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define IE(...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_INFO,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define N(...)                                                                 \
 tnc_log_log(LOG_MODULE,LOG_NOTICE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define NE(...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_NOTICE,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define W(...)                                                                 \
 tnc_log_log(LOG_MODULE,LOG_WARNING,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define WE(...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_WARNING,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define E(...)                                                                 \
 tnc_log_log(LOG_MODULE,LOG_ERR,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define EE(...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_ERR,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define C(...) do {                                                            \
 tnc_log_log(LOG_MODULE,LOG_CRIT,-1,__FILE__,__LINE__,__func__,__VA_ARGS__);   \
 exit(EXIT_FAILURE); } while (0)

#define CE(...) do {                                                           \
 tnc_log_log(LOG_MODULE,LOG_CRIT,errno,__FILE__,__LINE__,__func__,__VA_ARGS__);\
 exit(EXIT_FAILURE); } while (0)

/* special: always printed, never decorated */
#define A(...)                                                                 \
    tnc_log_log(-1,INT_MIN,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

/* tracing */
#if NOTRACE
# define T(...) do{}while(0)
# define TC(...) do{}while(0)
# define TR(...) do{}while(0)
#else
# define T(...)                                                                \
 tnc_log_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

# define TC(...)                                                               \
 do{                                                                           \
 tnc_log_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__);  \
 tnc_log_tcall();                                                              \
 } while (0)

# define TR(...)                                                               \
 do{                                                                           \
 tnc_log_tret();                                                               \
 tnc_log_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__);  \
 } while (0)
#endif

// ----- logger control interface -----

void tnc_log_setlvl_all(int lvl);
void tnc_log_setlvl(int mod, int lvl);
int  tnc_log_getlvl(int mod);

void tnc_log_setfancy(bool fancy);
bool tnc_log_getfancy(void);

void tnc_log_tret(void);
void tnc_log_tcall(void);

// ----- backend -----
void tnc_log_log(int mod, int lvl, int errn, const char *file, int line,
    const char *func, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 7, 8)))
#endif
    ;

void tnc_log_init(void);


#endif /* TELNETCAT_INTLOG_H */
