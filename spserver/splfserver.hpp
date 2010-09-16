/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __splfserver_hpp__
#define __splfserver_hpp__

#include "spthread.hpp"

class SP_EventArg;
class SP_ThreadPool;
class SP_HandlerFactory;
class SP_CompletionHandler;
class SP_IOChannelFactory;

typedef struct tagSP_AcceptArg SP_AcceptArg_t;

struct event;

// leader/follower thread pool server
class SP_LFServer {
public:
	SP_LFServer( const char * bindIP, int port, SP_HandlerFactory * handlerFactory );
	~SP_LFServer();

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
	char mBindIP[ 64 ];
	int mPort;
	int mIsShutdown;
	int mIsRunning;

	SP_AcceptArg_t * mAcceptArg;

	SP_EventArg * mEventArg;

	int mMaxThreads;
	SP_ThreadPool * mThreadPool;

	SP_CompletionHandler * mCompletionHandler;

	struct event * mEvAccept;
	struct event * mEvSigInt, * mEvSigTerm;

	sp_thread_mutex_t mMutex;

	void handleOneEvent();

	static void lfHandler( void * arg );

	static void sigHandler( int, short, void * arg );
};

#endif

