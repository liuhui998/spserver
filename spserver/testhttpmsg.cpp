/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "sphttpmsg.hpp"

void printMessage( SP_HttpMessage * message )
{
	for( int i = 0; i < message->getHeaderCount(); i++ ) {
		printf( "%s: %s\n", message->getHeaderName( i ), message->getHeaderValue( i ) );
	}
	printf( "\n" );

	if( NULL != message->getContent() ) {
		printf( "%s", (char*)message->getContent() );
	}

	printf( "\n" );
}

void printRequest( SP_HttpRequest * request )
{
	printf( "%s %s %s\n", request->getMethod(), request->getURI(), request->getVersion() );

	for( int i = 0; i < request->getParamCount(); i++ ) {
		printf( "Param: %s=%s\n", request->getParamName(i), request->getParamValue(i) );
	}

	printMessage( request );
}

void printResponse( SP_HttpResponse * response )
{
	printf( "%s %d %s\n", response->getVersion(), response->getStatusCode(),
		response->getReasonPhrase() );
	printMessage( response );
}

int main( int argc, char * argv[] )
{
	char * filename = NULL;

	if( argc < 2 ) {
		printf( "Usage: %s <file>\n", argv[0] );
		exit( -1 );
	} else {
		filename = argv[1];
	}

	FILE * fp = fopen ( filename, "r" );
	if( NULL == fp ) {
		printf( "cannot not open %s\n", filename );
		exit( -1 );
	}

	struct stat aStat;
	char * source = NULL;
	stat( filename, &aStat );
	source = ( char * ) malloc ( aStat.st_size + 1 );
	fread ( source, aStat.st_size, sizeof ( char ), fp );
	fclose ( fp );
	source[ aStat.st_size ] = 0;

	SP_HttpMsgParser parser;
	//parser.setIgnoreContent( 1 );

	int parsedLen = 0;
	for( int i = 0; i < (int)strlen( source ); i++ ) {
		parsedLen += parser.append( source + parsedLen, i - parsedLen + 1 );
		//printf( "%d, %d\n", i, parsedLen );
	}

	printf( "source length : %d, parsed length : %d\n", strlen( source ), parsedLen );

	printf( "parse complete : %s\n", parser.isCompleted() ? "Yes" : "No" );

	printf( "ignore content: %s\n", parser.isIgnoreContent() ? "Yes" : "No" );

	puts( "\n" );

	if( NULL != parser.getRequest() ) {
		printRequest( parser.getRequest() );
	}

	if( NULL != parser.getResponse() ) {
		printResponse( parser.getResponse() );
	}

	free( source );

	return 0;
}

