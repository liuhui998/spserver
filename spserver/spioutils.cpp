/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "spporting.hpp"

#include "spioutils.hpp"

void SP_IOUtils :: inetNtoa( in_addr * addr, char * ip, int size )
{
#if defined (linux) || defined (__sgi) || defined (__hpux) \
		|| defined (__FreeBSD__) || defined (__APPLE__) 
	const unsigned char *p = ( const unsigned char *) addr;
	snprintf( ip, size, "%i.%i.%i.%i", p[0], p[1], p[2], p[3] );
#else
	snprintf( ip, size, "%i.%i.%i.%i", addr->s_net, addr->s_host, addr->s_lh, addr->s_impno );
#endif
}

int SP_IOUtils :: setNonblock( int fd )
{
#ifdef WIN32
	unsigned long nonblocking = 1;
	ioctlsocket( fd, FIONBIO, (unsigned long*) &nonblocking );
#else
	int flags;

	flags = fcntl( fd, F_GETFL );
	if( flags < 0 ) return flags;

	flags |= O_NONBLOCK;
	if( fcntl( fd, F_SETFL, flags ) < 0 ) return -1;
#endif

	return 0;
}

int SP_IOUtils :: setBlock( int fd )
{
#ifdef WIN32

	unsigned long nonblocking = 0;
	ioctlsocket( fd, FIONBIO, (unsigned long*) &nonblocking );

#else

	int flags;

	flags = fcntl( fd, F_GETFL );
	if( flags < 0 ) return flags;

	flags &= ~O_NONBLOCK;
	if( fcntl( fd, F_SETFL, flags ) < 0 ) return -1;
#endif

	return 0;
}

int SP_IOUtils :: tcpListen( const char * ip, int port, int * fd, int blocking )
{
	int ret = 0;

	int listenFd = socket( AF_INET, SOCK_STREAM, 0 );
	if( listenFd < 0 ) {
		sp_syslog( LOG_WARNING, "socket failed, errno %d, %s", errno, strerror( errno ) );
		ret = -1;
	}

	if( 0 == ret && 0 == blocking ) {
		if( setNonblock( listenFd ) < 0 ) {
			sp_syslog( LOG_WARNING, "failed to set socket to non-blocking" );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		int flags = 1;
		if( setsockopt( listenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&flags, sizeof( flags ) ) < 0 ) {
			sp_syslog( LOG_WARNING, "failed to set setsock to reuseaddr" );
			ret = -1;
		}
		if( setsockopt( listenFd, IPPROTO_TCP, TCP_NODELAY, (char*)&flags, sizeof(flags) ) < 0 ) {
			sp_syslog( LOG_WARNING, "failed to set socket to nodelay" );
			ret = -1;
		}
	}

	struct sockaddr_in addr;

	if( 0 == ret ) {
		memset( &addr, 0, sizeof( addr ) );
		addr.sin_family = AF_INET;
		addr.sin_port = htons( port );

		addr.sin_addr.s_addr = INADDR_ANY;
		if( '\0' != *ip ) {
			if( 0 == sp_inet_aton( ip, &addr.sin_addr ) ) {
				sp_syslog( LOG_WARNING, "failed to convert %s to inet_addr", ip );
				ret = -1;
			}
		}
	}

	if( 0 == ret ) {
		if( bind( listenFd, (struct sockaddr*)&addr, sizeof( addr ) ) < 0 ) {
			sp_syslog( LOG_WARNING, "bind failed, errno %d, %s", errno, strerror( errno ) );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		if( ::listen( listenFd, 1024 ) < 0 ) {
			sp_syslog( LOG_WARNING, "listen failed, errno %d, %s", errno, strerror( errno ) );
			ret = -1;
		}
	}

	if( 0 != ret && listenFd >= 0 ) sp_close( listenFd );

	if( 0 == ret ) {
		* fd = listenFd;
		sp_syslog( LOG_NOTICE, "Listen on port [%d]", port );
	}

	return ret;
}

int SP_IOUtils :: tcpListen( const char * path, int * fd, int blocking, int mode )
{
	int ret = 0;

#ifndef WIN32

	struct sockaddr_un addr;
	memset( &addr, 0, sizeof( addr ) );

	if( strlen( path ) > ( sizeof( addr.sun_path ) - 1 ) ) {
		sp_syslog( LOG_WARNING, "UNIX socket name %s too long", path );
		return -1;
	}

	if( 0 == access( path, F_OK ) ) {
		if( 0 != unlink( path ) ) {
			sp_syslog( LOG_WARNING, "unlink %s failed, errno %d, %s",
				path, errno, strerror( errno ) );
			return -1;
		}
	}

	addr.sun_family = AF_UNIX;
	strncpy( addr.sun_path, path, sizeof( addr.sun_path ) - 1 );

	int listenFd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( listenFd < 0 ) {
		sp_syslog( LOG_WARNING, "listen failed, errno %d, %s", errno, strerror( errno ) );
		ret = -1;
	}

	if( 0 == ret && 0 == blocking ) {
		if( setNonblock( listenFd ) < 0 ) {
			sp_syslog( LOG_WARNING, "failed to set socket to non-blocking" );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		int flags = 1;
		if( setsockopt( listenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&flags, sizeof( flags ) ) < 0 ) {
			sp_syslog( LOG_WARNING, "failed to set setsock to reuseaddr" );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		if( bind( listenFd, (struct sockaddr*)&addr, sizeof( addr ) ) < 0 ) {
			sp_syslog( LOG_WARNING, "bind failed, errno %d, %s", errno, strerror( errno ) );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		if( ::listen( listenFd, 1024 ) < 0 ) {
			sp_syslog( LOG_WARNING, "listen failed, errno %d, %s", errno, strerror( errno ) );
			ret = -1;
		}
	}

	if( 0 != ret && listenFd >= 0 ) sp_close( listenFd );

	if( 0 == ret ) {
		* fd = listenFd;

		if( mode > 0 ) {
			if( 0 != fchmod( *fd, (mode_t)mode ) ) {
				sp_syslog( LOG_WARNING, "fchmod fail, errno %d, %s", errno, strerror( errno ) );
			}
		}

		sp_syslog( LOG_NOTICE, "Listen on [%s]", path );
	}

#endif

	return ret;
}

int SP_IOUtils :: initDaemon( const char * workdir )
{
#ifndef WIN32

	int		i;
	pid_t	pid;

	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		_exit(0);			/* parent terminates */

	/* child 1 continues... */

	if (setsid() < 0)			/* become session leader */
		return (-1);

	assert( signal( SIGHUP,  SIG_IGN ) != SIG_ERR );
	assert( signal( SIGPIPE, SIG_IGN ) != SIG_ERR );
	assert( signal( SIGALRM, SIG_IGN ) != SIG_ERR );
	assert( signal( SIGCHLD, SIG_IGN ) != SIG_ERR );

	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		_exit(0);			/* child 1 terminates */

	/* child 2 continues... */

	if( NULL != workdir ) chdir( workdir );		/* change working directory */

	/* close off file descriptors */
	for (i = 0; i < 64; i++)
		close(i);

	/* redirect stdin, stdout, and stderr to /dev/null */
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

#endif

	return (0);				/* success */
}

