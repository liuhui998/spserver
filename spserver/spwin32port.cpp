/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdarg.h>

#include "spwin32port.hpp"

#include <lm.h>
#include <tlhelp32.h>

/* Windows doesn't have writev() but does have WSASend */
int spwin32_writev(SOCKET sock, const struct iovec *vector, DWORD count)
{
	DWORD sent = -1;
	WSASend(sock, (LPWSABUF)vector, count, &sent, 0, NULL, NULL);
	return sent;
}

int spwin32_inet_aton(const char *c, struct in_addr* addr)
{
	unsigned int r;
	if (strcmp(c, "255.255.255.255") == 0) {
		addr->s_addr = 0xFFFFFFFFu;
		return 1;
	}
	r = inet_addr(c);
	if (r == INADDR_NONE)
		return 0;
	addr->s_addr = r;
	return 1;
}

int spwin32_socketpair(int d, int type, int protocol, int socks[2])
{
    struct sockaddr_in addr;
    SOCKET listener;
    int e;
    int addrlen = sizeof(addr);
    DWORD flags = 0;
	
    if (socks == 0) {
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
    }
	
    socks[0] = socks[1] = INVALID_SOCKET;
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) 
        return SOCKET_ERROR;
	
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0x7f000001);
    addr.sin_port = 0;
	
    e = bind(listener, (const struct sockaddr*) &addr, sizeof(addr));
    if (e == SOCKET_ERROR) {
        e = WSAGetLastError();
		closesocket(listener);
        WSASetLastError(e);
        return SOCKET_ERROR;
    }
    e = getsockname(listener, (struct sockaddr*) &addr, &addrlen);
    if (e == SOCKET_ERROR) {
        e = WSAGetLastError();
		closesocket(listener);
        WSASetLastError(e);
        return SOCKET_ERROR;
    }
	
    do {
        if (listen(listener, 1) == SOCKET_ERROR) break;
        if ((socks[0] = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags)) == INVALID_SOCKET) break;
        if (connect(socks[0], (const struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) break;
        if ((socks[1] = accept(listener, NULL, NULL)) == INVALID_SOCKET) break;
        closesocket(listener);
        return 0;
    } while (0);
    e = WSAGetLastError();
    closesocket(listener);
    closesocket(socks[0]);
    closesocket(socks[1]);
    WSASetLastError(e);
    return SOCKET_ERROR;
}

int spwin32_gettimeofday(struct timeval* tv, void * ) 
{
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch*/
		tmpres /= 10;  /*convert into microseconds*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS; 
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	return 0;
}

//-------------------------------------------------------------------

spwin32_logger_t g_spwin32_syslog = spwin32_syslog;

void spwin32_syslog (int priority, const char * format, ...)
{
	char logTemp[ 1024 ] = { 0 };

	va_list vaList;
	va_start( vaList, format );
	_vsnprintf( logTemp, sizeof( logTemp ), format, vaList );
	va_end ( vaList );

	if( strchr( logTemp, '\n' ) ) {
		printf( "#%d %s", GetCurrentThreadId(), logTemp );
	} else {
		printf( "#%d %s\n", GetCurrentThreadId(), logTemp );
	}
}

void spwin32_closelog (void)
{
}

void spwin32_openlog (const char *ident , int option , int facility)
{
}

int spwin32_setlogmask (int priority)
{
	return 0;
}

//-------------------------------------------------------------------

int spwin32_initsocket()
{
	WSADATA wsaData;
	
	int err = WSAStartup( MAKEWORD( 2, 0 ), &wsaData );
	if ( err != 0 ) {
		spwin32_syslog( LOG_EMERG, "Couldn't find a useable winsock.dll." );
		return -1;
	}

	return 0;
}

DWORD spwin32_getppid(void)
{
	HANDLE snapshot;
	PROCESSENTRY32 entry;
	DWORD myid = GetCurrentProcessId(), parentid = 0;
	int found = FALSE;

	snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if (snapshot == INVALID_HANDLE_VALUE) {
		sp_syslog( LOG_ERR, "couldn't take process snapshot in getppid()" );
		return parentid;
    }

	entry.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(snapshot, &entry)) {
		CloseHandle(snapshot);
		sp_syslog( LOG_ERR, "Process32First failed in getppid()" );
		return parentid;
    }

	do {
		if (entry.th32ProcessID == myid) {
			parentid = entry.th32ParentProcessID;
			found = TRUE;
			break;
		}
	} while (Process32Next(snapshot, &entry));

	CloseHandle(snapshot);

	if (!found) {
		sp_syslog( LOG_ERR, "couldn't find the current process entry in getppid()" );
	}

	return parentid;
}

const char * spwin32_strerror( DWORD lastError, char * errmsg, size_t len )
{
	if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, lastError, 0,
			errmsg, len - 1, NULL)) {
		/* if we fail, call ourself to find out why and return that error */
		return spwin32_strerror( GetLastError(), errmsg, len );  
	}

	return errmsg;
}

int spwin32_getexefile( DWORD pid, char * path, int size )
{
	HANDLE snapshot;
	MODULEENTRY32 entry;

	snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, pid );
	if (snapshot == INVALID_HANDLE_VALUE) {
		sp_syslog( LOG_ERR, "couldn't take process snapshot in getexefile()" );
		return -1;
    }

	entry.dwSize = sizeof(MODULEENTRY32);
	if (!Module32First(snapshot, &entry)) {
		CloseHandle(snapshot);
		char errmsg[ 256 ] = { 0 };
		sp_syslog( LOG_ERR, "Module32First failed in getexefile(), errno %d, %s",
			GetLastError(), spwin32_strerror( GetLastError(), errmsg, sizeof( errmsg ) ) );
		return -1;
    }

	::strncpy( path, entry.szExePath, size );
	path[ size - 1 ] = '\0';

	CloseHandle(snapshot);

	return 0;
}

void spwin32_pwd( char * path, int size )
{
	spwin32_getexefile( GetCurrentProcessId(), path, size );

	char * pos = strrchr( path, '\\' );
	if( NULL != pos ) *pos = '\0';
}

void spwin32_pause_console()
{
	DWORD ppid = spwin32_getppid();
	if( ppid > 0 ) {
		char filePath[ 256 ] = { 0 };
		spwin32_getexefile( ppid, filePath, sizeof( filePath ) );

		char * pos = strrchr( filePath, '\\' );
		if( NULL == pos ) pos = filePath;

		if( 0 == strcasecmp( pos + 1, "msdev.exe" )
				|| 0 == strcasecmp( pos + 1, "explorer.exe" ) )
		{
			printf( "\npress any key to exit ...\n" );
			getchar();
		}
	}
}
