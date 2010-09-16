/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "spporting.hpp"

#include "splfserver.hpp"

#include "speventcb.hpp"
#include "spthreadpool.hpp"
#include "sphandler.hpp"
#include "spexecutor.hpp"
#include "sputils.hpp"
#include "spioutils.hpp"
#include "spiochannel.hpp"

#include "event_msgqueue.h"

SP_LFServer :: SP_LFServer( const char * bindIP, int port, SP_HandlerFactory * handlerFactory )
{
	snprintf( mBindIP, sizeof( mBindIP ), "%s", bindIP );
	mPort = port;

	mIsShutdown = 0;
	mIsRunning = 0;

	mEventArg = new SP_EventArg( 600 );
	mMaxThreads = 4;

	mAcceptArg = (SP_AcceptArg_t*)malloc( sizeof( SP_AcceptArg_t ) );
	memset( mAcceptArg, 0, sizeof( SP_AcceptArg_t ) );
	mAcceptArg->mMaxConnections = 256;
	mAcceptArg->mReqQueueSize = 128;
	mAcceptArg->mRefusedMsg = strdup( "System busy, try again later." );
	mAcceptArg->mHandlerFactory = handlerFactory;

	mAcceptArg->mEventArg = mEventArg;

	mThreadPool = NULL;

	mEvAccept = mEvSigTerm = mEvSigInt = NULL;

	mCompletionHandler = NULL;

	sp_thread_mutex_init( &mMutex, NULL );
}

SP_LFServer :: ~SP_LFServer()
{
	shutdown();

	if( NULL != mThreadPool ) delete mThreadPool;
	mThreadPool = NULL;

	if( NULL != mCompletionHandler ) delete mCompletionHandler;
	mCompletionHandler = NULL;

	event_del( mEvAccept );
	free( mEvAccept );
	mEvAccept = NULL;

	signal_del( mEvSigTerm );
	free( mEvSigTerm );
	mEvSigTerm = NULL;

	signal_del( mEvSigInt );
	free( mEvSigInt );
	mEvSigInt = NULL;

	delete mAcceptArg->mHandlerFactory;
	free( mAcceptArg->mRefusedMsg );

	free( mAcceptArg );
	mAcceptArg = NULL;

	delete mEventArg;
	mEventArg = NULL;

	sp_thread_mutex_destroy( &mMutex );
}

void SP_LFServer :: setTimeout( int timeout )
{
	mEventArg->setTimeout( timeout );
}

void SP_LFServer :: setMaxConnections( int maxConnections )
{
	mAcceptArg->mMaxConnections = maxConnections > 0 ?
			maxConnections : mAcceptArg->mMaxConnections;
}

void SP_LFServer :: setMaxThreads( int maxThreads )
{
	mMaxThreads = maxThreads > 0 ? maxThreads : mMaxThreads;
}

void SP_LFServer :: setReqQueueSize( int reqQueueSize, const char * refusedMsg )
{
	mAcceptArg->mReqQueueSize = reqQueueSize > 0 ?
			reqQueueSize : mAcceptArg->mReqQueueSize;

	if( NULL != mAcceptArg->mRefusedMsg ) free( mAcceptArg->mRefusedMsg );
	mAcceptArg->mRefusedMsg = strdup( refusedMsg );
}

void SP_LFServer :: setIOChannelFactory( SP_IOChannelFactory * ioChannelFactory )
{
	mAcceptArg->mIOChannelFactory = ioChannelFactory;
}

void SP_LFServer :: shutdown()
{
	mIsShutdown = 1;
}

int SP_LFServer :: isRunning()
{
	return mIsRunning;
}

void SP_LFServer :: sigHandler( int, short, void * arg )
{
	SP_LFServer * server = (SP_LFServer*)arg;
	server->shutdown();
}

void SP_LFServer :: lfHandler( void * arg )
{
	SP_LFServer * server = (SP_LFServer*)arg;

	for( ; 0 == server->mIsShutdown; ) {
		server->handleOneEvent();
	}
}

void SP_LFServer :: handleOneEvent()
{
	SP_Task * task = NULL;
	SP_Message * msg = NULL;

	sp_thread_mutex_lock( &mMutex );

	for( ; 0 == mIsShutdown && NULL == task && NULL == msg; ) {
		if( mEventArg->getInputResultQueue()->getLength() > 0 ) {
			task = (SP_Task*)mEventArg->getInputResultQueue()->pop();
		} else if( mEventArg->getOutputResultQueue()->getLength() > 0 ) {
			msg = (SP_Message*)mEventArg->getOutputResultQueue()->pop();
		}

		if( NULL == task && NULL == msg ) {
			event_base_loop( mEventArg->getEventBase(), EVLOOP_ONCE );
		}
	}

	sp_thread_mutex_unlock( &mMutex );

	if( NULL != task ) task->run();

	if( NULL != msg ) mCompletionHandler->completionMessage( msg );
}

int SP_LFServer :: run()
{
#ifdef SIGPIPE
	/* Don't die with SIGPIPE on remote read shutdown. That's dumb. */
	signal( SIGPIPE, SIG_IGN );
#endif

	int ret = 0;
	int listenFD = -1;

	ret = SP_IOUtils::tcpListen( mBindIP, mPort, &listenFD, 0 );

	if( 0 == ret ) {
		// Clean close on SIGINT or SIGTERM.
		mEvSigInt = (struct event*)malloc( sizeof( struct event ) );
		signal_set( mEvSigInt, SIGINT, sigHandler, this );
		event_base_set( mEventArg->getEventBase(), mEvSigInt );
		signal_add( mEvSigInt, NULL);

		mEvSigTerm = (struct event*)malloc( sizeof( struct event ) );
		signal_set( mEvSigTerm, SIGTERM, sigHandler, this );
		event_base_set( mEventArg->getEventBase(), mEvSigTerm );
		signal_add( mEvSigTerm, NULL);

		mEvAccept = (struct event*)malloc( sizeof( struct event ) );
		event_set( mEvAccept, listenFD, EV_READ|EV_PERSIST,
				SP_EventCallback::onAccept, mAcceptArg );
		event_base_set( mEventArg->getEventBase(), mEvAccept );
		event_add( mEvAccept, NULL );

		mCompletionHandler = mAcceptArg->mHandlerFactory->createCompletionHandler();

		if( NULL == mAcceptArg->mIOChannelFactory ) {
			mAcceptArg->mIOChannelFactory = new SP_DefaultIOChannelFactory();
		}

		mThreadPool = new SP_ThreadPool( mMaxThreads );
		for( int i = 0; i < mMaxThreads; i++ ) {
			mThreadPool->dispatch( lfHandler, this );
		}
	}

	return ret;
}

void SP_LFServer :: runForever()
{
	run();
	pause();
}

