/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include "spporting.hpp"

#include "spmsgdecoder.hpp"
#include "spbuffer.hpp"

#include "spserver.hpp"
#include "splfserver.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"

#include "spsmtp.hpp"

class SP_FakeSmtpHandler : public SP_SmtpHandler {
public:
	SP_FakeSmtpHandler(){
		mAuthResult = 1;
	}

	virtual ~SP_FakeSmtpHandler() {}

	int ehlo( const char * args, SP_Buffer * reply )
	{
		reply->append( "250-OK\n" );
		reply->append( "250-AUTH LOGIN\n" );
		reply->append( "250 HELP\n" );

		return eAccept;
	}

	int auth( const char * user, const char * pass, SP_Buffer * reply )
	{
		//printf( "auth user %s, pass %s\n", user, pass );

		reply->append( "235 Authentication successful.\r\n" );
		mAuthResult = 1;

		return eAccept;
	}

	virtual int from( const char * args, SP_Buffer * reply ) {

		//printf( "mail from: %s\n", args );

		if( 0 == mAuthResult ) {
			reply->append( "503 Error: need AUTH command\r\n" );
			return eReject;
		}

		char buffer[ 128 ] = { 0 };
		snprintf( buffer, sizeof( buffer ), "250 %s, sender ok\r\n", args );
		reply->append( buffer );

		return eAccept;
	}

	virtual int rcpt( const char * args, SP_Buffer * reply ) {

		//printf( "rcpt to: %s\n", args );

		char buffer[ 128 ] = { 0 };
		snprintf( buffer, sizeof( buffer ), "250 %s, recipient ok\r\n", args );
		reply->append( buffer );

		return eAccept;
	}

	virtual int data( const char * data, SP_Buffer * reply ) {

		//printf( "data length %d\n", strlen( data ) );

		reply->append( "250 Requested mail action okay, completed.\r\n" );

		return eAccept;
	}

	virtual int rset( SP_Buffer * reply ) {
		reply->append( "250 OK\r\n" );

		return eAccept;
	}

private:
	int mAuthResult;
};

//---------------------------------------------------------

class SP_FakeSmtpHandlerFactory : public SP_SmtpHandlerFactory {
public:
	SP_FakeSmtpHandlerFactory();
	virtual ~SP_FakeSmtpHandlerFactory();

	virtual SP_SmtpHandler * create() const;

	//use default SP_CompletionHandler is enough, not need to implement
	//virtual SP_CompletionHandler * createCompletionHandler() const;
};

SP_FakeSmtpHandlerFactory :: SP_FakeSmtpHandlerFactory()
{
}

SP_FakeSmtpHandlerFactory :: ~SP_FakeSmtpHandlerFactory()
{
}

SP_SmtpHandler * SP_FakeSmtpHandlerFactory :: create() const
{
	return new SP_FakeSmtpHandler();
}

//---------------------------------------------------------

int main( int argc, char * argv[] )
{
	int port = 1025, maxThreads = 10;
	const char * serverType = "hahs";

#ifndef WIN32
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
#endif

	sp_openlog( "testsmtp", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );

	assert( 0 == sp_initsock() );

	if( 0 == strcasecmp( serverType, "hahs" ) ) {
		SP_Server server( "", port, new SP_SmtpHandlerAdapterFactory( new SP_FakeSmtpHandlerFactory() ) );

		server.setMaxConnections( 2048 );
		server.setTimeout( 600 );
		server.setMaxThreads( maxThreads );
		server.setReqQueueSize( 100, "Sorry, server is busy now!\n" );

		server.runForever();
	} else {
		SP_LFServer server( "", port, new SP_SmtpHandlerAdapterFactory( new SP_FakeSmtpHandlerFactory() ) );

		server.setMaxConnections( 2048 );
		server.setTimeout( 600 );
		server.setMaxThreads( maxThreads );
		server.setReqQueueSize( 100, "Sorry, server is busy now!\n" );

		server.runForever();
	}

	sp_closelog();

	return 0;
}

