/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>

#include "spiocpevent.hpp"
#include "sputils.hpp"
#include "spsession.hpp"

SP_IocpEventHeap :: SP_IocpEventHeap()
{
	mEntries = NULL;
	mMaxCount = mCount = 0;
}

SP_IocpEventHeap :: ~SP_IocpEventHeap()
{
	if( NULL != mEntries ) free( mEntries );
	mEntries = NULL;
	mMaxCount = mCount = 0;
}

int SP_IocpEventHeap :: getCount()
{
	return mCount;
}

int SP_IocpEventHeap :: push( SP_IocpEvent_t * item )
{
	if( 0 != reserve( mCount + 1 ) ) return -1;

	shiftUp( mCount++, item );

	return 0;
}

SP_IocpEvent_t * SP_IocpEventHeap :: top()
{
	return mCount ? mEntries[ 0 ] : NULL;
}

SP_IocpEvent_t * SP_IocpEventHeap :: pop()
{
	if( mCount ) {
		SP_IocpEvent_t * ret = mEntries[ 0 ];
		shiftDown( 0, mEntries[ --mCount ] );
		ret->mHeapIndex = -1;

		return ret;
	}

	return NULL;
}

int SP_IocpEventHeap :: erase( SP_IocpEvent_t * item )
{
	if( -1 != item->mHeapIndex ) {
		shiftDown( item->mHeapIndex, mEntries[ -- mCount ] );
		item->mHeapIndex = -1;

		return 0;
	}

	return -1;
}

int SP_IocpEventHeap :: reserve( int count )
{
	if( mMaxCount < count ) {
		int maxCount = mMaxCount ? mMaxCount *  2 : 8;
		if( maxCount < count ) maxCount = count;

		SP_IocpEvent_t ** p = (SP_IocpEvent_t**)realloc( mEntries, maxCount * sizeof( SP_IocpEvent_t ) );
		if( NULL == p ) return -1;

		mEntries = p;
		mMaxCount = maxCount;
	}

	return 0;

}

int SP_IocpEventHeap :: isGreater( SP_IocpEvent_t * item1, SP_IocpEvent_t * item2 )
{
	if( item1->mTimeout.tv_sec == item2->mTimeout.tv_sec ) {
		return item1->mTimeout.tv_usec > item2->mTimeout.tv_usec;
	} else {
		return item1->mTimeout.tv_sec > item2->mTimeout.tv_sec;
	}
}

void SP_IocpEventHeap :: shiftUp( int index, SP_IocpEvent_t * item )
{
	int parent = ( index - 1 ) / 2;

	for( ; index && isGreater( mEntries[ parent ], item ); ) {
		mEntries[ index ] = mEntries[ parent ];
		mEntries[ index ]->mHeapIndex = index;
		index = parent;
		parent = ( index - 1 ) / 2;
	}
	mEntries[ index ] = item;
	item->mHeapIndex = index;
}

void SP_IocpEventHeap :: shiftDown( int index, SP_IocpEvent_t * item )
{
	int minChild = 2 * ( index + 1 );
	for( ; minChild <= mCount; ) {
		minChild -= ( minChild == mCount || isGreater( mEntries[ minChild ], mEntries[ minChild - 1 ] ) );
		if( ! isGreater( item, mEntries[ minChild ] ) ) break;

		mEntries[ index ] = mEntries[ minChild ];
		mEntries[ index ]->mHeapIndex = index;
		index = minChild;
		minChild = 2 * ( index + 1 );
	}
	shiftUp( index, item );
}

//===================================================================

SP_IocpMsgQueue :: SP_IocpMsgQueue( HANDLE completionPort,
		DWORD completionKey, QueueFunc_t func, void * arg )
{
	mCompletionPort = completionPort;
	mCompletionKey = completionKey;
	mFunc = func;
	mArg = arg;

	mMutex = CreateMutex( NULL, FALSE, NULL );
	mQueue = new SP_CircleQueue();
}

SP_IocpMsgQueue :: ~SP_IocpMsgQueue()
{
	CloseHandle( mMutex );
	delete mQueue;
	mQueue = NULL;
}

int SP_IocpMsgQueue :: push( void * queueData )
{
	WaitForSingleObject( mMutex, INFINITE );

	mQueue->push( queueData );
	if( 1 == mQueue->getLength() ) {
		PostQueuedCompletionStatus( mCompletionPort, 0, mCompletionKey, (OVERLAPPED*)this );
	}

	ReleaseMutex( mMutex );

	return 0;
}

int SP_IocpMsgQueue :: process()
{
	WaitForSingleObject( mMutex, INFINITE );

	for( ; mQueue->getLength() > 0; ) {
		void * queueData = mQueue->pop();

		ReleaseMutex( mMutex );

		mFunc( queueData, mArg );		

		WaitForSingleObject( mMutex, INFINITE );
	}

	ReleaseMutex( mMutex );

	return 0;
}

//===================================================================

SP_IocpEventArg :: SP_IocpEventArg( int timeout )
{
	mInputResultQueue = new SP_BlockingQueue();
	mOutputResultQueue = new SP_BlockingQueue();

	mResponseQueue = NULL;
	
	mSessionManager = new SP_SessionManager();

	mEventHeap = new SP_IocpEventHeap();

	mTimeout = timeout;

	mCompletionPort = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if( NULL == mCompletionPort ) {
		sp_syslog( LOG_ERR, "CreateIoCompletionPort failed, errno %d", WSAGetLastError() );
	}

	mDisconnectExFunc = NULL;
}

SP_IocpEventArg :: ~SP_IocpEventArg()
{
	if( NULL != mInputResultQueue ) delete mInputResultQueue;
	mInputResultQueue = NULL;

	if( NULL != mOutputResultQueue ) delete mOutputResultQueue;
	mOutputResultQueue = NULL;

	if( NULL != mEventHeap ) delete mEventHeap;
	mEventHeap = NULL;

	if( NULL != mResponseQueue ) delete mResponseQueue;
	mResponseQueue = NULL;

	if( NULL != mSessionManager ) delete mSessionManager;
	mSessionManager = NULL;
}

HANDLE SP_IocpEventArg :: getCompletionPort()
{
	return mCompletionPort;
}

SP_BlockingQueue * SP_IocpEventArg :: getInputResultQueue()
{
	return mInputResultQueue;
}
	
SP_BlockingQueue * SP_IocpEventArg :: getOutputResultQueue()
{
	return mOutputResultQueue;
}

void SP_IocpEventArg :: setResponseQueue( SP_IocpMsgQueue * responseQueue )
{
	mResponseQueue = responseQueue;
}

SP_IocpMsgQueue * SP_IocpEventArg :: getResponseQueue()
{
	return mResponseQueue;
}

SP_SessionManager * SP_IocpEventArg :: getSessionManager()
{
	return mSessionManager;
}

SP_IocpEventHeap * SP_IocpEventArg :: getEventHeap()
{
	return mEventHeap;
}

void SP_IocpEventArg :: setTimeout( int timeout )
{
	mTimeout = timeout;
}

int SP_IocpEventArg :: getTimeout()
{
	return mTimeout;
}

int SP_IocpEventArg :: loadDisconnectEx( SOCKET fd )
{
	LPFN_DISCONNECTEX fnDisConnectEx = NULL;
	GUID guidDisConnectEx = WSAID_DISCONNECTEX;
	DWORD dwByte;
	::WSAIoctl( fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidDisConnectEx, sizeof(guidDisConnectEx),
			&fnDisConnectEx, sizeof(fnDisConnectEx),
			&dwByte, NULL, NULL); 

	mDisconnectExFunc = fnDisConnectEx;

	return NULL != mDisconnectExFunc ? 0 : -1;
}

BOOL SP_IocpEventArg :: disconnectEx( SOCKET fd, LPOVERLAPPED lpOverlapped,
		DWORD dwFlags, DWORD reserved )
{
	LPFN_DISCONNECTEX fnDisConnectEx = (LPFN_DISCONNECTEX)mDisconnectExFunc;
	if( NULL != fnDisConnectEx ) {
		return fnDisConnectEx( fd, lpOverlapped, dwFlags, reserved );
	}

	return FALSE;
}
