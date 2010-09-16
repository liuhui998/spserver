/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>

#include "spmsgdecoder.hpp"
#include "spbuffer.hpp"

#include "spserver.hpp"
#include "splfserver.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"

#define MAXN    16384           /* max # bytes client can request */
#define MAXLINE         4096    /* max text line length */

class SP_UnpHandler : public SP_Handler {
public:
	SP_UnpHandler(){}
	virtual ~SP_UnpHandler(){}

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response ) {
		request->setMsgDecoder( new SP_LineMsgDecoder() );
		return 0;
	}

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response ) {
		SP_LineMsgDecoder * decoder = (SP_LineMsgDecoder*)request->getMsgDecoder();

		int ntowrite = atol( (char*)decoder->getMsg() );

		if ((ntowrite <= 0) || (ntowrite > MAXN)) {
			syslog( LOG_WARNING, "WARN: client request for %d bytes", ntowrite );
			return -1;
		}

		char result[ MAXN ];

		response->getReply()->getMsg()->append( result, ntowrite );

		return -1;
	}

	virtual void error( SP_Response * response ) {}

	virtual void timeout( SP_Response * response ) {}

	virtual void close() {}
};

class SP_UnpHandlerFactory : public SP_HandlerFactory {
public:
	SP_UnpHandlerFactory() {}
	virtual ~SP_UnpHandlerFactory() {}

	virtual SP_Handler * create() const {
		return new SP_UnpHandler();
	}
};

//---------------------------------------------------------

int main( int argc, char * argv[] )
{
	int port = 1770, maxThreads = 10;
	const char * serverType = "lf";

	extern char *optarg ;
	int c ;

	while( ( c = getopt ( argc, argv, "p:t:s:v" )) != EOF ) {
		switch ( c ) {
			case 'p' :
				port = atoi( optarg );
				break;
			case 't':
				maxThreads = atoi( optarg );
				break;
			case 's':
				serverType = optarg;
				break;
			case '?' :
			case 'v' :
				printf( "Usage: %s [-p <port>] [-t <threads>] [-s <hahs|lf>]\n", argv[0] );
				exit( 0 );
		}
	}

	openlog( "testunp", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );

	setlogmask( LOG_UPTO( LOG_WARNING ) );

	if( 0 == strcasecmp( serverType, "hahs" ) ) {
		SP_Server server( "", port, new SP_UnpHandlerFactory() );

		server.setTimeout( 60 );
		server.setMaxThreads( maxThreads );
		server.setReqQueueSize( 100, "Sorry, server is busy now!\r\n" );

		server.runForever();
	} else {
		SP_LFServer server( "", port, new SP_UnpHandlerFactory() );

		server.setTimeout( 60 );
		server.setMaxThreads( maxThreads );
		server.setReqQueueSize( 100, "Sorry, server is busy now!\r\n" );

		server.runForever();
	}

	closelog();

	return 0;
}

