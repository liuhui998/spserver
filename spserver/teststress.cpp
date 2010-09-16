/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#ifdef WIN32
#include "spgetopt.h"
#endif

#include "spporting.hpp"

#include "event.h"

static const char * gHost = "127.0.0.1";
static int gPort = 3333;
static int gMsgs = 10;
static int gClients = 10;
static int gConnWait = 0;
static int gSockWait = 0;

static time_t gStartTime = 0;

struct SP_TestClient {
	int mFd;
	struct event mReadEvent;
	struct event mWriteEvent;
	int mSendMsgs;
	int mRecvMsgs;
	char mBuffer[ 512 ];
};

void showUsage( const char * program )
{
	printf( "\nStress Test Tools for spserver example -- testecho/testchat\n\n" );
	printf( "Usage: %s [-h <host>] [-p <port>] [-c <clients>] [-m <messages>]\n"
			"\t\t\t[-w <connect wait>] [-s <socket wait>]\n\n", program );
	printf( "\t-h default is %s\n", gHost );
	printf( "\t-p default is %d\n", gPort );
	printf( "\t-c how many clients, default is %d\n", gClients );
	printf( "\t-m messages per client, default is %d\n", gMsgs );
	printf( "\t-w how many milliseconds to wait between creating every 100 connections, default is %d\n", gConnWait );
	printf( "\t-s how many milliseconds to wait between every event loop, default is %d\n", gSockWait );
	printf( "\n" );
}

void close_read( SP_TestClient * client )
{
	//fprintf( stderr, "#%d close read\n", client->mFd );
	event_del( &client->mReadEvent );
	gClients--;
}

void close_write( SP_TestClient * client )
{
	//fprintf( stderr, "#%d close write\n", client->mFd );
	event_del( &client->mWriteEvent );
}

void close_client( SP_TestClient * client )
{
	close_write( client );
	close_read( client );
}

void on_read( int fd, short events, void *arg )
{
	SP_TestClient * client = ( SP_TestClient * ) arg;

	if( EV_READ & events ) {
		int len = recv( fd, client->mBuffer, sizeof( client->mBuffer ), 0 );
		if( len <= 0 ) {
			if( len < 0 && EAGAIN != errno ) {
				fprintf( stderr, "#%d on_read error, count %d, errno %d, %s\n",
						fd, client->mRecvMsgs, errno, strerror( errno ) );
			}
			close_client( client );
		} else {
			for( int i = 0; i < len; i++ ) {
				//if( 10 == fd ) printf( "%c", client->mBuffer[i] );
				if( '\n' == client->mBuffer[i] ) client->mRecvMsgs++;
			}
		}
	} else {
		fprintf( stderr, "#%d on_read timeout\n", fd );
		close_client( client );
	}
}

void on_write( int fd, short events, void *arg )
{
	SP_TestClient * client = ( SP_TestClient * ) arg;

	if( EV_WRITE & events ) {
		client->mSendMsgs++;

		if( client->mSendMsgs >= gMsgs ) {
			snprintf( client->mBuffer, sizeof( client->mBuffer ), "quit\n" );
		} else {
			snprintf( client->mBuffer, sizeof( client->mBuffer ),
				"mail #%d, It's good to see how people hire; "
				"that tells us how to market ourselves to them.\n", client->mSendMsgs );
		}

		int len = send( fd, client->mBuffer, strlen( client->mBuffer ), 0 );

		if( len <= 0 && EAGAIN != errno ) {
			fprintf( stderr, "#%d on_write error, errno %d, %s\n", fd, errno, strerror( errno ) );
			close_client( client );
		} else {
			if( client->mSendMsgs >= gMsgs ) close_write( client );
		}
	} else {
		fprintf( stderr, "#%d on_write timeout\n", fd );
		close_client( client );
	}
}

void parse_arg( int argc, char * argv[] )
{
	extern char *optarg ;
	int c ;

	while( ( c = getopt ( argc, argv, "h:p:c:m:w:s:v" )) != EOF ) {
		switch ( c ) {
			case 'h' :
				gHost = optarg;
				break;
			case 'p':
				gPort = atoi( optarg );
				break;
			case 'c' :
				gClients = atoi ( optarg );
				break;
			case 'm' :
				gMsgs = atoi( optarg );
				break;
			case 'w':
				gConnWait = atoi( optarg );
				break;
			case 's':
				gSockWait = atoi( optarg );
				break;
			case 'v' :
			case '?' :
				showUsage( argv[0] );
				exit( 0 );
		}
	}
}

int main( int argc, char * argv[] )
{
	parse_arg( argc, argv );

#ifdef SIGPIPE
	signal( SIGPIPE, SIG_IGN );
#endif

	sp_initsock();

	event_init();

	SP_TestClient * clientList = (SP_TestClient*)calloc( gClients, sizeof( SP_TestClient ) );

	struct sockaddr_in sin;
	memset( &sin, 0, sizeof(sin) );
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr( gHost );
	sin.sin_port = htons( gPort );

	int totalClients = gClients, i = 0;

	printf( "Create %d connections to server, it will take some minutes to complete.\n", gClients );
	for( i = 0; i < gClients; i++ ) {
		SP_TestClient * client = clientList + i;

		client->mFd = socket( AF_INET, SOCK_STREAM, 0 );
		if( client->mFd < 0 ) {
			fprintf(stderr, "#%d, socket failed, errno %d, %s\n", i, errno, strerror( errno ) );
#ifdef WIN32
			spwin32_pause_console();
#endif
			return -1;
		}

		if( connect( client->mFd, (struct sockaddr *)&sin, sizeof(sin) ) != 0) {
			fprintf(stderr, "#%d, connect failed, errno %d, %s\n", i, errno, strerror( errno ) );
#ifdef WIN32
			spwin32_pause_console();
#endif
			return -1;
		}

		event_set( &client->mWriteEvent, client->mFd, EV_WRITE | EV_PERSIST, on_write, client );
		event_add( &client->mWriteEvent, NULL );

		event_set( &client->mReadEvent, client->mFd, EV_READ | EV_PERSIST, on_read, client );
		event_add( &client->mReadEvent, NULL );

		if( 0 == ( i % 10 ) ) write( fileno( stdout ), ".", 1 );

		if( gConnWait > 0 && ( i > 0 ) && ( 0 == ( i % 100 ) ) ) usleep( gConnWait * 1000 );
	}

	time( &gStartTime );

	struct timeval startTime, stopTime;

	sp_gettimeofday( &startTime, NULL );

	time_t lastInfoTime = time( NULL );

	// start event loop until all clients are exit
	while( gClients > 0 ) {
		event_loop( EVLOOP_ONCE );

		if( time( NULL ) - lastInfoTime > 5 ) {
			time( &lastInfoTime );
			printf( "waiting for %d client(s) to exit\n", gClients );
		}

		if( gSockWait > 0 ) usleep( gSockWait * 1000 );
	}

	sp_gettimeofday( &stopTime, NULL );

	double totalTime = (double) ( 1000000 * ( stopTime.tv_sec - startTime.tv_sec )
			+ ( stopTime.tv_usec - startTime.tv_usec ) ) / 1000000;

	// show result
	printf( "\n\nTest result :\n" );
	printf( "Host %s, Port %d, ConnWait: %d, SockWait: %d\n", gHost, gPort, gConnWait, gSockWait );
	printf( "Clients : %d, Messages Per Client : %d\n", totalClients, gMsgs );
	printf( "ExecTimes: %.6f seconds\n\n", totalTime );

	printf( "client\tSend\tRecv\n" );
	int totalSend = 0, totalRecv = 0;
	for( i = 0; i < totalClients; i++ ) {
		SP_TestClient * client = clientList + i;

		//printf( "client#%d : %d\t%d\n", i, client->mSendMsgs, client->mRecvMsgs );

		totalSend += client->mSendMsgs;
		totalRecv += client->mRecvMsgs;

		sp_close( client->mFd );
	}

	printf( "total   : %d\t%d\n", totalSend, totalRecv );
	printf( "average : %.0f/s\t%.0f/s\n", totalSend / totalTime, totalRecv / totalTime );

	free( clientList );

#ifdef WIN32
	spwin32_pause_console();
#endif

	return 0;
}

