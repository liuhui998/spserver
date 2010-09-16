/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <stdio.h>

#include "spporting.hpp"

#include "sprequest.hpp"
#include "spmsgdecoder.hpp"
#include "sputils.hpp"

SP_Request :: SP_Request()
{
	mDecoder = new SP_DefaultMsgDecoder();

	memset( mClientIP, 0, sizeof( mClientIP ) );
	mClientPort = 0;

	memset( mServerIP, 0, sizeof( mServerIP ) );
}

SP_Request :: ~SP_Request()
{
	if( NULL != mDecoder ) delete mDecoder;
	mDecoder = NULL;
}

SP_MsgDecoder * SP_Request :: getMsgDecoder()
{
	return mDecoder;
}

void SP_Request :: setMsgDecoder( SP_MsgDecoder * decoder )
{
	if( NULL != mDecoder ) delete mDecoder;
	mDecoder = decoder;
}

void SP_Request :: setClientIP( const char * clientIP )
{
	sp_strlcpy( mClientIP, clientIP, sizeof( mClientIP ) );
}

const char * SP_Request :: getClientIP()
{
	return mClientIP;
}

void SP_Request :: setClientPort( int port )
{
	mClientPort = port;
}

int SP_Request :: getClientPort()
{
	return mClientPort;
}

void SP_Request :: setServerIP( const char * ip )
{
	sp_strlcpy( mServerIP, ip, sizeof( mServerIP ) );
}

const char * SP_Request :: getServerIP()
{
	return mServerIP;
}

