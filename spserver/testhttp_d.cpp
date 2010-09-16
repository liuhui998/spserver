/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include "spporting.hpp"

#include "sphttp.hpp"
#include "sphttpmsg.hpp"
#include "spdispatcher.hpp"
#include "spioutils.hpp"

class SP_HttpEchoHandler : public SP_HttpHandler {
public:
	SP_HttpEchoHandler(){}
	virtual ~SP_HttpEchoHandler(){}

	virtual void handle( SP_HttpRequest * request, SP_HttpResponse * response ) {
		response->setStatusCode( 200 );
		response->appendContent( "<html><head>"
			"<title>Welcome to simple http</title>"
			"</head><body>" );

		char buffer[ 512 ] = { 0 };
		snprintf( buffer, sizeof( buffer ),
			"<p>The requested URI is : %s.</p>", request->getURI() );
		response->appendContent( buffer );

		snprintf( buffer, sizeof( buffer ),
			"<p>Client IP is : %s.</p>", request->getClientIP() );
		response->appendContent( buffer );

		int i = 0;

		for( i = 0; i < request->getParamCount(); i++ ) {
			snprintf( buffer, sizeof( buffer ),
				"<p>Param - %s = %s<p>", request->getParamName( i ), request->getParamValue( i ) );
			response->appendContent( buffer );
		}

		for( i = 0; i < request->getHeaderCount(); i++ ) {
			snprintf( buffer, sizeof( buffer ),
				"<p>Header - %s: %s<p>", request->getHeaderName( i ), request->getHeaderValue( i ) );
			response->appendContent( buffer );
		}

		if( NULL != request->getContent() ) {
			response->appendContent( "<p>" );
			response->appendContent( request->getContent(), request->getContentLength() );
			response->appendContent( "</p>" );
		}

		response->appendContent( "</body></html>\n" );
	}
};

class SP_HttpEchoHandlerFactory : public SP_HttpHandlerFactory {
public:
	SP_HttpEchoHandlerFactory(){}
	virtual ~SP_HttpEchoHandlerFactory(){}

	virtual SP_HttpHandler * create() const {
		return new SP_HttpEchoHandler();
	}
};

int main( int argc, char * argv[] )
{
	int port = 8080, maxThreads = 10;

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

	//openlog( "testhttp_d", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );

	int maxConnections = 10000, reqQueueSize = 10000;
	const char * refusedMsg = "HTTP/1.1 500 Sorry, server is busy now!\r\n";

	SP_HttpHandlerAdapterFactory factory( new SP_HttpEchoHandlerFactory() );

	int listenFd = -1;
	if( 0 == SP_IOUtils::tcpListen( "", port, &listenFd ) ) {
		SP_Dispatcher dispatcher( new SP_DefaultCompletionHandler(), maxThreads );
		dispatcher.dispatch();
		dispatcher.setTimeout( 60 );

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
					dispatcher.push( fd, factory.create() );
				}
			} else {
				break;
			}
		}
	}

	closelog();

	return 0;
}

