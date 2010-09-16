/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "spdispatcher.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"
#include "spbuffer.hpp"
#include "spmsgdecoder.hpp"
#include "speventcb.hpp"
#include "spioutils.hpp"

class SP_EchoHandler : public SP_Handler {
public:
	SP_EchoHandler(){}
	virtual ~SP_EchoHandler(){}

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response ) {
		request->setMsgDecoder( new SP_LineMsgDecoder() );
		response->getReply()->getMsg()->append(
			"Welcome to line echo dispatcher, enter 'quit' to quit.\r\n" );

		return 0;
	}

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response ) {
		SP_LineMsgDecoder * decoder = (SP_LineMsgDecoder*)request->getMsgDecoder();

		if( 0 != strcasecmp( (char*)decoder->getMsg(), "quit" ) ) {
			response->getReply()->getMsg()->append( (char*)decoder->getMsg() );
			response->getReply()->getMsg()->append( "\r\n" );
			return 0;
		} else {
			response->getReply()->getMsg()->append( "Byebye\r\n" );
			return -1;
		}
	}

	virtual void error( SP_Response * response ) {}

	virtual void timeout( SP_Response * response ) {}

	virtual void close() {}
};

class SP_EchoTimerHandler : public SP_TimerHandler {
public:
	SP_EchoTimerHandler(){
		mCount = 1;
	}

	virtual ~SP_EchoTimerHandler(){}

	// return -1 : terminate timer, 0 : continue
	virtual int handle( SP_Response * response, struct timeval * timeout ) {
		syslog( LOG_NOTICE, "time = %li, call timer handler", time( NULL ) );

		if( ++mCount >= 10 ) {
			syslog( LOG_NOTICE, "stop timer" );
			return -1;
		} else {
			syslog( LOG_NOTICE, "set timer to %d seconds later", mCount );
			timeout->tv_sec = mCount;
			return 0;
		}
	}

private:
	int mCount;
};

int main( int argc, char * argv[] )
{
	int port = 3333, maxThreads = 10;

	extern char *optarg ;
	int c ;

	while( ( c = getopt ( argc, argv, "p:t:v" )) != EOF ) {
		switch ( c ) {
			case 'p' :
				port = atoi( optarg );
				break;
			case 't':
				maxThreads = atoi( optarg );
				break;
			case '?' :
			case 'v' :
				printf( "Usage: %s [-p <port>] [-t <threads>]\n", argv[0] );
				exit( 0 );
		}
	}

	openlog( "testdispatcher", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );

	int maxConnections = 100, reqQueueSize = 10;
	const char * refusedMsg = "System busy, try again later.";

	int listenFd = -1;
	if( 0 == SP_IOUtils::tcpListen( "", port, &listenFd ) ) {
		SP_Dispatcher dispatcher( new SP_DefaultCompletionHandler(), maxThreads );
		dispatcher.dispatch();

		struct timeval timeout;
		memset( &timeout, 0, sizeof( timeout ) );
		timeout.tv_sec = 1;

		dispatcher.push( &timeout, new SP_EchoTimerHandler() );

		for( ; ; ) {
			struct sockaddr_in addr;
			socklen_t socklen = sizeof( addr );
			int fd = accept( listenFd, (struct sockaddr*)&addr, &socklen );

			if( fd > 0 ) {
				if( dispatcher.getSessionCount() >= maxConnections
						|| dispatcher.getReqQueueLength() >= reqQueueSize ) {
					write( fd, refusedMsg, strlen( refusedMsg ) );
					close( fd );
				} else {
					dispatcher.push( fd, new SP_EchoHandler() );
				}
			} else {
				break;
			}
		}
	}

	closelog();

	return 0;
}

