/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spwin32port_hpp__
#define __spwin32port_hpp__

#pragma warning(disable: 4996)

#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <io.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned short uint16_t;
typedef unsigned __int64 uint64_t;
typedef int socklen_t;

#ifndef WSAID_DISCONNECTEX
	#define WSAID_DISCONNECTEX {0x7fda2e11,0x8630,0x436f,{0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57}}
	typedef BOOL (WINAPI *LPFN_DISCONNECTEX)(SOCKET, LPOVERLAPPED, DWORD, DWORD);
#endif

#if _MSC_VER >= 1400
#define localtime_r(_clock, _result) localtime_s(_result, _clock)
#define gmtime_r(_clock, _result) gmtime_s(_result, _clock)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#define localtime_r(_clock, _result) ( *(_result) = *localtime( (_clock) ), (_result) )
#define gmtime_r(_clock, _result) ( *(_result) = *gmtime( (_clock) ), (_result) )
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#define snprintf _snprintf
#define pause()	Sleep((32767L << 16) + 32767)
#define sleep(x) Sleep(x*1000)

#define sp_close        closesocket
#define sp_writev       spwin32_writev
#define sp_inet_aton    spwin32_inet_aton
#define sp_socketpair   spwin32_socketpair
#define sp_initsock     spwin32_initsocket
#define sp_gettimeofday spwin32_gettimeofday

#define sp_syslog       g_spwin32_syslog
#define sp_openlog      spwin32_openlog
#define sp_closelog     spwin32_closelog
#define sp_setlogmask   spwin32_setlogmask

/* Windows writev() support */
struct iovec
{
	u_long	iov_len;
	char	*iov_base;
};

extern int spwin32_writev(SOCKET sock, const struct iovec *vector, DWORD count);

extern int spwin32_inet_aton(const char *c, struct in_addr* addr);

extern int spwin32_socketpair(int d, int type, int protocol, int sv[2]);

/* @return >0 OK, 0 FAIL */
extern DWORD spwin32_getppid(void);

/* @return 0 OK, -1 Fail*/
extern int spwin32_getexefile( DWORD pid, char * path, int size );

extern const char * spwin32_strerror( DWORD lastError, char * errmsg, size_t len );

extern void spwin32_pwd( char * path, int size );

/* @return 0 OK, -1 Fail */
extern int spwin32_initsocket();

extern void spwin32_pause_console();

extern int spwin32_gettimeofday(struct timeval* tv, void * );

/* Windows syslog() support */

#define	LOG_EMERG	0
#define	LOG_ALERT	1
#define	LOG_CRIT	2
#define	LOG_ERR		3
#define	LOG_WARNING	4
#define	LOG_NOTICE	5
#define	LOG_INFO	6
#define	LOG_DEBUG	7

/*
 * Option flags for openlog.
 *
 * LOG_ODELAY no longer does anything.
 * LOG_NDELAY is the inverse of what it used to be.
 */
#define LOG_PID         0x01    /* log the pid with each message */
#define LOG_CONS        0x02    /* log on the console if errors in sending */
#define LOG_ODELAY      0x04    /* delay open until first syslog() (default) */
#define LOG_NDELAY      0x08    /* don't delay open */
#define LOG_NOWAIT      0x10    /* don't wait for console forks: DEPRECATED */
#define LOG_PERROR      0x20    /* log to stderr as well */

#define	LOG_USER	(1<<3)

extern void spwin32_syslog (int priority, const char * format, ...);
extern void spwin32_closelog (void);
extern void spwin32_openlog (const char *ident , int option , int facility);
extern int spwin32_setlogmask (int priority);

typedef void ( * spwin32_logger_t ) ( int priority, const char * format, ... );

/* default is spwin32_syslog, write to stdout */
extern spwin32_logger_t g_spwin32_syslog;

#ifdef __cplusplus
}
#endif

#endif
