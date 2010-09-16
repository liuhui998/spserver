/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>

#include "spporting.hpp"
#include "spthread.hpp"

#include "spiocpdispatcher.hpp"

#include "spwin32iocp.hpp"
#include "sphandler.hpp"
#include "spsession.hpp"
#include "spexecutor.hpp"
#include "sputils.hpp"
#include "spioutils.hpp"
#include "spiochannel.hpp"
#include "sprequest.hpp"

#include "spiocpevent.hpp"

SP_IocpDispatcher :: SP_IocpDispatcher( SP_CompletionHandler * completionHandler, int maxThreads )
{
#ifdef SIGPIPE
	/* Don't die with SIGPIPE on remote read shutdown. That's dumb. */
	signal( SIGPIPE, SIG_IGN );
#endif

	mIsShutdown = 0;
	mIsRunning = 0;

	mEventArg = new SP_IocpEventArg( 600 );
	SP_IocpMsgQueue * msgQueue = new SP_IocpMsgQueue( mEventArg->getCompletionPort(),
			SP_IocpEventCallback::eKeyMsgQueue, SP_IocpEventCallback::onResponse, mEventArg );
	mEventArg->setResponseQueue( msgQueue );
	// load DisconnectEx
	{
		int fd = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED );
		mEventArg->loadDisconnectEx( fd );
		closesocket( fd );
	}

	mMaxThreads = maxThreads > 0 ? maxThreads : 4;

	mCompletionHandler = completionHandler;

	mPushQueue = new SP_IocpMsgQueue( mEventArg->getCompletionPort(),
			SP_IocpEventCallback::eKeyMsgQueue, onPush, mEventArg );
}

SP_IocpDispatcher :: ~SP_IocpDispatcher()
{
	if( 0 == mIsRunning ) sleep( 1 );

	shutdown();

	for( ; mIsRunning; ) sleep( 1 );

	delete mPushQueue;
	mPushQueue = NULL;

	delete mEventArg;
	mEventArg = NULL;
}

void SP_IocpDispatcher :: setTimeout( int timeout )
{
	mEventArg->setTimeout( timeout );
}

void SP_IocpDispatcher :: shutdown()
{
	mIsShutdown = 1;

	PostQueuedCompletionStatus( mEventArg->getCompletionPort(), 0, 0, 0 );
}

int SP_IocpDispatcher :: isRunning()
{
	return mIsRunning;
}

int SP_IocpDispatcher :: getSessionCount()
{
	return mEventArg->getSessionManager()->getCount();
}

int SP_IocpDispatcher :: getReqQueueLength()
{
	return mEventArg->getInputResultQueue()->getLength();
}

int SP_IocpDispatcher :: dispatch()
{
	int ret = -1;

	sp_thread_attr_t attr;
	sp_thread_attr_init( &attr );
	assert( sp_thread_attr_setstacksize( &attr, 1024 * 1024 ) == 0 );
	sp_thread_attr_setdetachstate( &attr, SP_THREAD_CREATE_DETACHED );

	sp_thread_t thread;
	ret = sp_thread_create( &thread, &attr, eventLoop, this );
	sp_thread_attr_destroy( &attr );
	if( 0 == ret ) {
		sp_syslog( LOG_NOTICE, "Thread #%ld has been created for dispatcher", thread );
	} else {
		mIsRunning = 0;
		sp_syslog( LOG_WARNING, "Unable to create a thread for dispatcher, %s",
			strerror( errno ) ) ;
	}

	return ret;
}

sp_thread_result_t SP_THREAD_CALL SP_IocpDispatcher :: eventLoop( void * arg )
{
	SP_IocpDispatcher * dispatcher = (SP_IocpDispatcher*)arg;

	dispatcher->mIsRunning = 1;

	dispatcher->start();

	dispatcher->mIsRunning = 0;

	return 0;
}

void SP_IocpDispatcher :: outputCompleted( void * arg )
{
	SP_CompletionHandler * handler = ( SP_CompletionHandler * ) ((void**)arg)[0];
	SP_Message * msg = ( SP_Message * ) ((void**)arg)[ 1 ];

	handler->completionMessage( msg );

	free( arg );
}

int SP_IocpDispatcher :: start()
{
	SP_Executor workerExecutor( mMaxThreads, "work" );
	SP_Executor actExecutor( 1, "act" );

	/* Start the event loop. */
	while( 0 == mIsShutdown ) {
		SP_IocpEventCallback::eventLoop( mEventArg, NULL );

		for( ; NULL != mEventArg->getInputResultQueue()->top(); ) {
			SP_Task * task = (SP_Task*)mEventArg->getInputResultQueue()->pop();
			workerExecutor.execute( task );
		}

		for( ; NULL != mEventArg->getOutputResultQueue()->top(); ) {
			SP_Message * msg = (SP_Message*)mEventArg->getOutputResultQueue()->pop();

			void ** arg = ( void** )malloc( sizeof( void * ) * 2 );
			arg[ 0 ] = (void*)mCompletionHandler;
			arg[ 1 ] = (void*)msg;

			actExecutor.execute( outputCompleted, arg );
		}
	}

	sp_syslog( LOG_NOTICE, "Dispatcher is shutdown." );

	return 0;
}

typedef struct tagSP_IocpPushArg {
	int mType;      // 0 : fd, 1 : timer

	// for push fd
	int mFd;
	SP_Handler * mHandler;
	SP_IOChannel * mIOChannel;
	int mNeedStart;

	// for push timer
	struct timeval mTimeout;
	SP_IocpEvent_t mTimerEvent;
	SP_TimerHandler * mTimerHandler;
	SP_IocpEventArg * mEventArg;
	SP_IocpMsgQueue * mPushQueue;
} SP_IocpPushArg_t;

void SP_IocpDispatcher :: onPush( void * queueData, void * arg )
{
	SP_IocpPushArg_t * pushArg = (SP_IocpPushArg_t*)queueData;
	SP_IocpEventArg * eventArg = (SP_IocpEventArg*)arg;

	if( 0 == pushArg->mType ) {
		SP_Sid_t sid;
		sid.mKey = eventArg->getSessionManager()->allocKey( &sid.mSeq );
		assert( sid.mKey > 0 );

		SP_Session * session = new SP_Session( sid );

		char clientIP[ 32 ] = { 0 };
		{
			struct sockaddr_in clientAddr;
			socklen_t clientLen = sizeof( clientAddr );
			getpeername( pushArg->mFd, (struct sockaddr *)&clientAddr, &clientLen );
			SP_IOUtils::inetNtoa( &( clientAddr.sin_addr ), clientIP, sizeof( clientIP ) );
			session->getRequest()->setClientPort( ntohs( clientAddr.sin_port ) );
		}
		session->getRequest()->setClientIP( clientIP );

		session->setHandler( pushArg->mHandler );
		session->setArg( eventArg );
		session->setIOChannel( pushArg->mIOChannel );

		if( SP_IocpEventCallback::addSession( eventArg, (HANDLE)pushArg->mFd, session ) ) {
			eventArg->getSessionManager()->put( sid.mKey, sid.mSeq, session );

			if( pushArg->mNeedStart ) {
				SP_IocpEventHelper::doStart( session );
			} else {
				SP_IocpEventCallback::addRecv( session );
			}
		} else {
			delete session;
		}

		free( pushArg );
	} else {
		memset( &( pushArg->mTimerEvent ), 0, sizeof( SP_IocpEvent_t ) );
		pushArg->mTimerEvent.mType = SP_IocpEvent_t::eEventTimer;

		struct timeval curr;
		sp_gettimeofday( &curr, NULL );
		struct timeval * dest = &( pushArg->mTimerEvent.mTimeout );
		struct timeval * src = &( pushArg->mTimeout );

		dest->tv_sec = curr.tv_sec + src->tv_sec;
		dest->tv_usec = curr.tv_usec + src->tv_usec;
		if( dest->tv_usec >= 1000000 ) {
			dest->tv_sec++;
			dest->tv_usec -= 1000000;
		}

		pushArg->mTimerEvent.mHeapIndex = -1;
		pushArg->mTimerEvent.mOnTimer = onTimer;

		eventArg->getEventHeap()->push( &( pushArg->mTimerEvent ) );
	}
}

int SP_IocpDispatcher :: push( int fd, SP_Handler * handler, int needStart )
{
	SP_IOChannel * ioChannel = new SP_DefaultIOChannel();
	return push( fd, handler, ioChannel, needStart );
}

int SP_IocpDispatcher :: push( int fd, SP_Handler * handler,
		SP_IOChannel * ioChannel, int needStart )
{
	SP_IocpPushArg_t * arg = (SP_IocpPushArg_t*)malloc( sizeof( SP_IocpPushArg_t ) );
	arg->mType = 0;
	arg->mFd = fd;
	arg->mHandler = handler;
	arg->mNeedStart = needStart;
	arg->mIOChannel = ioChannel;

	SP_IOUtils::setNonblock( fd );

	return mPushQueue->push( arg );
}

void SP_IocpDispatcher :: onTimer( void * arg )
{
	SP_IocpEvent_t * event = (SP_IocpEvent_t*)arg;
	SP_IocpPushArg_t * pushArg = CONTAINING_RECORD( event, SP_IocpPushArg_t, mTimerEvent );

	pushArg->mEventArg->getInputResultQueue()->push(
		new SP_SimpleTask( timer, pushArg, 1 ) );
}

void SP_IocpDispatcher :: timer( void * arg )
{
	SP_IocpPushArg_t * pushArg = (SP_IocpPushArg_t*)arg;
	SP_TimerHandler * handler = pushArg->mTimerHandler;
	SP_IocpEventArg * eventArg = pushArg->mEventArg;

	SP_Sid_t sid;
	sid.mKey = SP_Sid_t::eTimerKey;
	sid.mSeq = SP_Sid_t::eTimerSeq;
	SP_Response * response = new SP_Response( sid );
	if( 0 == handler->handle( response, &( pushArg->mTimeout ) ) ) {
		pushArg->mPushQueue->push( arg );
	} else {
		delete pushArg->mTimerHandler;
		free( pushArg );
	}

	eventArg->getResponseQueue()->push( response );
}

int SP_IocpDispatcher :: push( const struct timeval * timeout, SP_TimerHandler * handler )
{
	SP_IocpPushArg_t * arg = (SP_IocpPushArg_t*)malloc( sizeof( SP_IocpPushArg_t ) );

	arg->mType = 1;
	arg->mTimeout = *timeout;
	arg->mTimerHandler = handler;
	arg->mEventArg = mEventArg;
	arg->mPushQueue = mPushQueue;

	return mPushQueue->push( arg );
}

int SP_IocpDispatcher :: push( SP_Response * response )
{
	return mEventArg->getResponseQueue()->push( response );
}
