/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __splfserver_hpp__
#define __splfserver_hpp__

#include "spthread.hpp"

class SP_IocpEventArg;
class SP_ThreadPool;
class SP_HandlerFactory;
class SP_CompletionHandler;
class SP_IOChannelFactory;

typedef struct tagSP_IocpAcceptArg SP_IocpAcceptArg_t;

// leader/follower thread pool server
class SP_IocpLFServer {
public:
	SP_IocpLFServer( const char * bindIP, int port, SP_HandlerFactory * handlerFactory );
	~SP_IocpLFServer();

	void setTimeout( int timeout );
	void setMaxConnections( int maxConnections );
	void setMaxThreads( int maxThreads );
	void setReqQueueSize( int reqQueueSize, const char * refusedMsg );
	void setIOChannelFactory( SP_IOChannelFactory * ioChannelFactory );

	void shutdown();
	int isRunning();

	// return -1 : cannot listen on ip:port, 0 : ok
	int run();

	void runForever();

private:
	HANDLE mCompletionPort;
	char mBindIP[ 64 ];
	int mPort;
	int mIsShutdown;
	int mIsRunning;

	SP_IocpAcceptArg_t * mAcceptArg;

	SP_IocpEventArg * mEventArg;

	int mMaxThreads;
	SP_ThreadPool * mThreadPool;

	SP_CompletionHandler * mCompletionHandler;

	sp_thread_mutex_t mMutex;

	void handleOneEvent();

	static void lfHandler( void * arg );

	static void sigHandler( int, short, void * arg );

	static sp_thread_result_t SP_THREAD_CALL acceptThread( void * arg );
};

#endif

