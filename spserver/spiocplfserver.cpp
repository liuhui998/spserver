/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "spporting.hpp"

#include "spiocplfserver.hpp"

#include "spiocpevent.hpp"
#include "spwin32iocp.hpp"
#include "spthreadpool.hpp"
#include "sphandler.hpp"
#include "spexecutor.hpp"
#include "sputils.hpp"
#include "spioutils.hpp"
#include "spiochannel.hpp"

SP_IocpLFServer :: SP_IocpLFServer( const char * bindIP, int port, SP_HandlerFactory * handlerFactory )
{
	snprintf( mBindIP, sizeof( mBindIP ), "%s", bindIP );
	mPort = port;

	mIsShutdown = 0;
	mIsRunning = 0;

	mEventArg = new SP_IocpEventArg( 600 );

	mMaxThreads = 4;

	mAcceptArg = (SP_IocpAcceptArg_t*)malloc( sizeof( SP_IocpAcceptArg_t ) );
	memset( mAcceptArg, 0, sizeof( SP_IocpAcceptArg_t ) );
	mAcceptArg->mMaxConnections = 256;
	mAcceptArg->mReqQueueSize = 128;
	mAcceptArg->mRefusedMsg = strdup( "System busy, try again later." );
	mAcceptArg->mHandlerFactory = handlerFactory;

	mAcceptArg->mEventArg = mEventArg;

	mThreadPool = NULL;

	mCompletionHandler = NULL;

	sp_thread_mutex_init( &mMutex, NULL );

	mCompletionPort = mEventArg->getCompletionPort();
}

SP_IocpLFServer :: ~SP_IocpLFServer()
{
	shutdown();

	for( ; mIsRunning; ) sleep( 1 );

	if( NULL != mThreadPool ) delete mThreadPool;
	mThreadPool = NULL;

	if( NULL != mCompletionHandler ) delete mCompletionHandler;
	mCompletionHandler = NULL;

	delete mAcceptArg->mHandlerFactory;
	free( mAcceptArg->mRefusedMsg );

	free( mAcceptArg );
	mAcceptArg = NULL;

	delete mEventArg;
	mEventArg = NULL;

	sp_thread_mutex_destroy( &mMutex );

	mCompletionPort = NULL;
}

void SP_IocpLFServer :: setTimeout( int timeout )
{
	mEventArg->setTimeout( timeout );
}

void SP_IocpLFServer :: setMaxConnections( int maxConnections )
{
	mAcceptArg->mMaxConnections = maxConnections > 0 ?
			maxConnections : mAcceptArg->mMaxConnections;
}

void SP_IocpLFServer :: setMaxThreads( int maxThreads )
{
	mMaxThreads = maxThreads > 0 ? maxThreads : mMaxThreads;
}

void SP_IocpLFServer :: setReqQueueSize( int reqQueueSize, const char * refusedMsg )
{
	mAcceptArg->mReqQueueSize = reqQueueSize > 0 ?
			reqQueueSize : mAcceptArg->mReqQueueSize;

	if( NULL != mAcceptArg->mRefusedMsg ) free( mAcceptArg->mRefusedMsg );
	mAcceptArg->mRefusedMsg = strdup( refusedMsg );
}

void SP_IocpLFServer :: setIOChannelFactory( SP_IOChannelFactory * ioChannelFactory )
{
	mAcceptArg->mIOChannelFactory = ioChannelFactory;
}

void SP_IocpLFServer :: shutdown()
{
	mIsShutdown = 1;
	
	if( NULL != mCompletionPort ) {
		PostQueuedCompletionStatus( mCompletionPort, 0, 0, 0 );
	}
}

int SP_IocpLFServer :: isRunning()
{
	return mIsRunning;
}

void SP_IocpLFServer :: sigHandler( int, short, void * arg )
{
	SP_IocpLFServer * server = (SP_IocpLFServer*)arg;
	server->shutdown();
}

void SP_IocpLFServer :: lfHandler( void * arg )
{
	SP_IocpLFServer * server = (SP_IocpLFServer*)arg;

	for( ; 0 == server->mIsShutdown; ) {
		server->handleOneEvent();
	}

	server->mIsRunning = 0;
}

void SP_IocpLFServer :: handleOneEvent()
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
			SP_IocpEventCallback::eventLoop( mEventArg, mAcceptArg );
		}
	}

	sp_thread_mutex_unlock( &mMutex );

	if( NULL != task ) task->run();

	if( NULL != msg ) mCompletionHandler->completionMessage( msg );
}

sp_thread_result_t SP_THREAD_CALL SP_IocpLFServer :: acceptThread( void * arg )
{
	DWORD recvBytes = 0;

	SP_IocpAcceptArg_t * acceptArg = (SP_IocpAcceptArg_t*)arg;
	
	for( ; ; ) {
		acceptArg->mClientSocket = (HANDLE)WSASocket( AF_INET, SOCK_STREAM,
				IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED );
		if( INVALID_SOCKET == (int)acceptArg->mClientSocket ) {
			sp_syslog( LOG_ERR, "WSASocket fail, errno %d", WSAGetLastError() );
			Sleep( 50 );
			continue;
		}

		SP_IOUtils::setNonblock( (int)acceptArg->mClientSocket );
		memset( &( acceptArg->mOverlapped ), 0, sizeof( OVERLAPPED ) );

		BOOL ret = AcceptEx( (SOCKET)acceptArg->mListenSocket, (SOCKET)acceptArg->mClientSocket,
				acceptArg->mBuffer,	0, sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16,
				&recvBytes, &( acceptArg->mOverlapped ) );

		int lastError = WSAGetLastError();
		if( FALSE == ret && (ERROR_IO_PENDING != lastError) ) {
			sp_syslog( LOG_ERR, "AcceptEx() fail, errno %d", lastError );
			closesocket( (int)acceptArg->mClientSocket );
			if( WSAENOBUFS == lastError ) Sleep( 50 );
		} else {
			WaitForSingleObject( acceptArg->mAcceptEvent, INFINITE );
			ResetEvent( acceptArg->mAcceptEvent );
		}
	}

	return 0;
}

int SP_IocpLFServer :: run()
{
	int ret = 0;
	int listenFD = -1;

	ret = SP_IOUtils::tcpListen( mBindIP, mPort, &listenFD, 0 );

	if( 0 == ret ) {
		mCompletionHandler = mAcceptArg->mHandlerFactory->createCompletionHandler();

		SP_IocpMsgQueue * msgQueue = new SP_IocpMsgQueue( mEventArg->getCompletionPort(),
			SP_IocpEventCallback::eKeyMsgQueue, SP_IocpEventCallback::onResponse, mEventArg );
		mEventArg->setResponseQueue( msgQueue );
		mEventArg->loadDisconnectEx( listenFD );

		mAcceptArg->mAcceptEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
		mAcceptArg->mListenSocket = (HANDLE)listenFD;

		if( NULL == CreateIoCompletionPort( mAcceptArg->mListenSocket,
				mEventArg->getCompletionPort(), SP_IocpEventCallback::eKeyAccept, 0 ) ) {
			sp_syslog( LOG_ERR, "CreateIoCompletionPort fail, errno %d", WSAGetLastError() );
			return -1;		
		}

		if( NULL == mAcceptArg->mIOChannelFactory ) {
			mAcceptArg->mIOChannelFactory = new SP_DefaultIOChannelFactory();
		}

		sp_thread_t thread;
		ret = sp_thread_create( &thread, NULL, acceptThread, mAcceptArg );
		if( 0 == ret ) {
			sp_syslog( LOG_NOTICE, "Thread #%ld has been created to accept socket", thread );
		} else {
			sp_syslog( LOG_WARNING, "Unable to create a thread to accept socket, %s", strerror( errno ) );
			return -1;
		}

		mIsRunning = 1;

		mThreadPool = new SP_ThreadPool( mMaxThreads );
		for( int i = 0; i < mMaxThreads; i++ ) {
			mThreadPool->dispatch( lfHandler, this );
		}
	}

	return ret;
}

void SP_IocpLFServer :: runForever()
{
	run();
	pause();
}

