/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <assert.h>

#include "spthread.hpp"

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "spporting.hpp"
#include "spwin32iocp.hpp"
#include "spiocpserver.hpp"

#include "spmsgdecoder.hpp"
#include "spbuffer.hpp"

#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"
#include "sputils.hpp"

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"mswsock")

class SP_OnlineSidList {
public:
	SP_OnlineSidList();
	~SP_OnlineSidList();

	void copy( SP_SidList * outList, SP_Sid_t * ignoreSid = NULL );
	void remove( SP_Sid_t sid );
	void add( SP_Sid_t sid );
	int getCount();

private:
	SP_SidList mList;
	sp_thread_mutex_t mMutex;
};

SP_OnlineSidList :: SP_OnlineSidList()
{
	sp_thread_mutex_init( &mMutex, NULL );
}

SP_OnlineSidList :: ~SP_OnlineSidList()
{
	sp_thread_mutex_destroy( &mMutex );
}

void SP_OnlineSidList :: copy( SP_SidList * outList, SP_Sid_t * ignoreSid )
{
	sp_thread_mutex_lock( &mMutex );

	for( int i = 0; i < mList.getCount(); i++ ) {
		if( NULL != ignoreSid ) {
			SP_Sid_t theSid = mList.get( i );
			if( theSid.mKey == ignoreSid->mKey && theSid.mSeq == ignoreSid->mSeq ) {
				continue;
			}
		}

		outList->add( mList.get( i ) );
	}

	sp_thread_mutex_unlock( &mMutex );
}

void SP_OnlineSidList :: remove( SP_Sid_t sid )
{
	sp_thread_mutex_lock( &mMutex );

	for( int i = 0; i < mList.getCount(); i++ ) {
		SP_Sid_t theSid = mList.get( i );
		if( theSid.mKey == sid.mKey && theSid.mSeq == sid.mSeq ) {
			mList.take( i );
			break;
		}
	}

	sp_thread_mutex_unlock( &mMutex );
}

void SP_OnlineSidList :: add( SP_Sid_t sid )
{
	sp_thread_mutex_lock( &mMutex );

	mList.add( sid );

	sp_thread_mutex_unlock( &mMutex );
}

int SP_OnlineSidList :: getCount()
{
	int count = 0;

	sp_thread_mutex_lock( &mMutex );

	count = mList.getCount();

	sp_thread_mutex_unlock( &mMutex );

	return count;
}

//---------------------------------------------------------

class SP_ChatHandler : public SP_Handler {
public:
	SP_ChatHandler( SP_OnlineSidList * onlineSidList );
	virtual ~SP_ChatHandler();

	virtual int start( SP_Request * request, SP_Response * response );

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response );

	virtual void error( SP_Response * response );

	virtual void timeout( SP_Response * response );

	virtual void close();

private:
	SP_Sid_t mSid;

	SP_OnlineSidList * mOnlineSidList;

	static int mMsgSeq;

	void broadcast( SP_Response * response, const char * buffer, SP_Sid_t * ignoreSid = 0 );
};

int SP_ChatHandler :: mMsgSeq = 0;

SP_ChatHandler :: SP_ChatHandler( SP_OnlineSidList * onlineSidList )
{
	memset( &mSid, 0, sizeof( mSid ) );

	mOnlineSidList = onlineSidList;
}

SP_ChatHandler :: ~SP_ChatHandler()
{
}

void SP_ChatHandler :: broadcast( SP_Response * response, const char * buffer, SP_Sid_t * ignoreSid )
{
	if( mOnlineSidList->getCount() > 0 ) {
		SP_Message * msg = new SP_Message();
		mOnlineSidList->copy( msg->getToList(), ignoreSid );
		msg->setCompletionKey( ++mMsgSeq );

		msg->getMsg()->append( buffer );
		response->addMessage( msg );
	}
}

int SP_ChatHandler :: start( SP_Request * request, SP_Response * response )
{
	request->setMsgDecoder( new SP_MultiLineMsgDecoder() );

	mSid = response->getFromSid();

	char buffer[ 128 ] = { 0 };
	snprintf( buffer, sizeof( buffer ),
		"Welcome %d to chat server, enter 'quit' to quit.\r\n", mSid.mKey );
	response->getReply()->getMsg()->append( buffer );
	response->getReply()->setCompletionKey( ++mMsgSeq );

	snprintf( buffer, sizeof( buffer ), "SYS : %d online\r\n", mSid.mKey);

	broadcast( response, buffer );

	mOnlineSidList->add( mSid );

	return 0;
}

int SP_ChatHandler :: handle( SP_Request * request, SP_Response * response )
{
	SP_MultiLineMsgDecoder * decoder = (SP_MultiLineMsgDecoder*)request->getMsgDecoder();
	SP_CircleQueue * msgQueue = decoder->getQueue();

	char buffer[ 256 ] = { 0 };
	int ret = 0;

	for( ; NULL != msgQueue->top(); ) {
		char * line = (char*)msgQueue->pop();
		if( 0 != strcasecmp( line, "quit" ) ) {
			snprintf( buffer, sizeof( buffer ), "%d say: %s\r\n", mSid.mKey, line );
			broadcast( response, buffer );
		} else {
			snprintf( buffer, sizeof( buffer ), "SYS : %d normal offline\r\n", mSid.mKey );
			broadcast( response, buffer, &mSid );

			response->getReply()->getMsg()->append( "SYS : Byebye\r\n" );
			response->getReply()->setCompletionKey( ++mMsgSeq );

			ret = -1;
		}

		free( line );
	}

	return ret;
}

void SP_ChatHandler :: error( SP_Response * response )
{
	char buffer[ 64 ] = { 0 };
	snprintf( buffer, sizeof( buffer ), "SYS : %d error offline\r\n", mSid.mKey );

	broadcast( response, buffer, &mSid );
}

void SP_ChatHandler :: timeout( SP_Response * response )
{
	char buffer[ 64 ] = { 0 };
	snprintf( buffer, sizeof( buffer ), "SYS : %d timeout offline\r\n", mSid.mKey );

	broadcast( response, buffer, &mSid );
}

void SP_ChatHandler :: close()
{
	mOnlineSidList->remove( mSid );
}

//---------------------------------------------------------

class SP_ChatCompletionHandler : public SP_CompletionHandler {
public:
	SP_ChatCompletionHandler();
	~SP_ChatCompletionHandler();
	virtual void completionMessage( SP_Message * msg );
};

SP_ChatCompletionHandler :: SP_ChatCompletionHandler()
{
}

SP_ChatCompletionHandler :: ~SP_ChatCompletionHandler()
{
}

void SP_ChatCompletionHandler :: completionMessage( SP_Message * msg )
{
#if 0
	printf( "message completed { completion key : %d }\n", msg->getCompletionKey() );

	printf( "\tsuccess {" );
	for( int i = 0; i < msg->getSuccess()->getCount(); i++ ) {
		printf( " %d", msg->getSuccess()->get( i ).mKey );
	}
	printf( "}\n" );

	printf( "\tfailure {" );
	for( int i = 0; i < msg->getFailure()->getCount(); i++ ) {
		printf( " %d", msg->getFailure()->get( i ).mKey );
	}
	printf( "}\n" );
#endif

	delete msg;
}

//---------------------------------------------------------

class SP_ChatHandlerFactory : public SP_HandlerFactory {
public:
	SP_ChatHandlerFactory( SP_OnlineSidList * onlineSidList );
	virtual ~SP_ChatHandlerFactory();

	virtual SP_Handler * create() const;

	virtual SP_CompletionHandler * createCompletionHandler() const;

private:
	SP_OnlineSidList * mOnlineSidList;
};

SP_ChatHandlerFactory :: SP_ChatHandlerFactory( SP_OnlineSidList * onlineSidList )
{
	mOnlineSidList = onlineSidList;
}

SP_ChatHandlerFactory :: ~SP_ChatHandlerFactory()
{
}

SP_Handler * SP_ChatHandlerFactory :: create() const
{
	return new SP_ChatHandler( mOnlineSidList );
}

SP_CompletionHandler * SP_ChatHandlerFactory :: createCompletionHandler() const
{
	return new SP_ChatCompletionHandler();
}

//---------------------------------------------------------

int main( int argc, char * argv[] )
{
	int port = 5555, maxThreads = 10;

	if( 0 != sp_initsock() ) assert( 0 );

	SP_OnlineSidList onlineSidList;

	SP_IocpServer server( "", port, new SP_ChatHandlerFactory( &onlineSidList ) );
	server.setTimeout( 60 );
	server.setMaxThreads( maxThreads );
	server.setReqQueueSize( 100, "Sorry, server is busy now!\n" );

	server.runForever();

	sp_closelog();

	return 0;
}

