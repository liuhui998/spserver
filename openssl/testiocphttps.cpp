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
#include "spiocpserver.hpp"
#include "spopenssl.hpp"
#include "spgetopt.h"

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

void sp_getbasepath( char * path, int size )
{
	spwin32_getexefile( GetCurrentProcessId(), path, size );

	char * pos = strrchr( path, '\\' );
	if( NULL != pos ) *pos = '\0';
}

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

	if( 0 != sp_initsock() ) assert( 0 );

	char basePath[ 256 ] = { 0 }, crtPath[ 256 ] = { 0 }, keyPath[ 256 ] = { 0 };
	sp_getbasepath( basePath, sizeof( basePath ) );
	snprintf( crtPath, sizeof( crtPath ), "%s\\..\\..\\..\\openssl\\demo.crt", basePath );
	snprintf( keyPath, sizeof( keyPath ), "%s\\..\\..\\..\\openssl\\demo.key", basePath );

	SP_OpensslChannelFactory * opensslFactory = new SP_OpensslChannelFactory();
	if( 0 != opensslFactory->init( crtPath, keyPath ) ) assert( 0 );
 
	SP_IocpServer server( "", port, new SP_HttpHandlerAdapterFactory( new SP_HttpEchoHandlerFactory() ) );

	server.setTimeout( 60 );
	server.setMaxThreads( maxThreads );
	server.setReqQueueSize( 100, "HTTP/1.1 500 Sorry, server is busy now!\r\n" );
	server.setIOChannelFactory( opensslFactory );

	server.runForever();

	return 0;
}

