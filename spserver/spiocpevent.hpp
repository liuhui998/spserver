/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spiocpevent_hpp__
#define __spiocpevent_hpp__

#include "spporting.hpp"

typedef struct tagSP_IocpEvent {
	enum { SP_IOCP_MAX_IOV = 8 };
	enum { eEventUnknown, eEventRecv, eEventSend, eEventTimer };

	OVERLAPPED mOverlapped;
	int mType;
	WSABUF mWsaBuf;

	void ( * mOnTimer ) ( void * );

	int mHeapIndex;
	struct timeval mTimeout;
} SP_IocpEvent_t;

class SP_IocpEventHeap {
public:
	SP_IocpEventHeap();
	~SP_IocpEventHeap();

	int push( SP_IocpEvent_t * item );

	SP_IocpEvent_t * top();

	SP_IocpEvent_t * pop();

	int erase( SP_IocpEvent_t * item );

	int getCount();

private:

	static int isGreater( SP_IocpEvent_t * item1, SP_IocpEvent_t * item2 );

	int reserve( int count );

	void shiftUp( int index, SP_IocpEvent_t * item );
	void shiftDown( int index, SP_IocpEvent_t * item );

	SP_IocpEvent_t ** mEntries;
	int mMaxCount, mCount;
};

class SP_CircleQueue;
class SP_BlockingQueue;
class SP_SessionManager;

class SP_IocpMsgQueue {
public:
	typedef void ( * QueueFunc_t ) ( void * queueData, void * arg );

	SP_IocpMsgQueue( HANDLE completionPort, DWORD completionKey, QueueFunc_t func, void * arg );
	~SP_IocpMsgQueue();

	int push( void * queueData );

	int process();

private:
	HANDLE mCompletionPort;
	DWORD mCompletionKey;
	QueueFunc_t mFunc;
	void * mArg;

	HANDLE mMutex;
	SP_CircleQueue * mQueue;
};

class SP_IocpEventArg {
public:
	SP_IocpEventArg( int timeout );
	~SP_IocpEventArg();

	HANDLE getCompletionPort();
	SP_BlockingQueue * getInputResultQueue();
	SP_BlockingQueue * getOutputResultQueue();

	void setResponseQueue( SP_IocpMsgQueue * responseQueue );
	SP_IocpMsgQueue * getResponseQueue();

	SP_SessionManager * getSessionManager();

	SP_IocpEventHeap * getEventHeap();

	int loadDisconnectEx( SOCKET fd );

	BOOL disconnectEx( SOCKET fd, LPOVERLAPPED lpOverlapped,
			DWORD dwFlags, DWORD reserved );

	void setTimeout( int timeout );
	int getTimeout();

private:
	SP_BlockingQueue * mInputResultQueue;
	SP_BlockingQueue * mOutputResultQueue;
	SP_IocpMsgQueue * mResponseQueue;

	SP_SessionManager * mSessionManager;

	SP_IocpEventHeap * mEventHeap;

	void * mDisconnectExFunc;

	int mTimeout;

	HANDLE mCompletionPort;
};

#endif

