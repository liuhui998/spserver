/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spiocpdispatcher_hpp__
#define __spiocpdispatcher_hpp__

#include "spporting.hpp"
#include "spthread.hpp"

class SP_CompletionHandler;
class SP_Handler;
class SP_Message;
class SP_BlockingQueue;
class SP_TimerHandler;
class SP_IOChannel;
class SP_Response;

class SP_IocpEventArg;
class SP_IocpMsgQueue;

class SP_IocpDispatcher {
public:
	SP_IocpDispatcher( SP_CompletionHandler * completionHandler, int maxThreads = 64 );
	~SP_IocpDispatcher();

	void setTimeout( int timeout );

	int getSessionCount();
	int getReqQueueLength();

	void shutdown();
	int isRunning();

	/**
	 * @brief  create a thread to run event loop
	 * @return 0 : OK, -1 : Fail, cannot create thread
	 */
	int dispatch();

	/**
	 * @brief register a fd into dispatcher
	 * @param needStart : 1 - call handler::start, 0 - don't call handler::start
	 * @return 0 : OK, -1 : Fail, invalid fd
	 * @note  handler will be deleted by dispatcher when the session is close
	 */
	int push( int fd, SP_Handler * handler, int needStart = 1 );

	int push( int fd, SP_Handler * handler, SP_IOChannel * ioChannel, int needStart = 1 );

	/**
	 * @brief register a timer into dispatcher
	 * @param timeout : the interval for the timer
	 * @note  handler will be deleted by dispatcher when the timer is terminated
	 */
	int push( const struct timeval * timeout, SP_TimerHandler * handler );

	/**
	 * @brief push a response
	 */
	int push( SP_Response * response );

private:
	int mIsShutdown;
	int mIsRunning;
	int mMaxThreads;

	SP_IocpEventArg * mEventArg;
	SP_CompletionHandler * mCompletionHandler;

	SP_IocpMsgQueue * mPushQueue;

	int start();

	static sp_thread_result_t SP_THREAD_CALL eventLoop( void * arg );

	static void onPush( void * queueData, void * arg );

	static void outputCompleted( void * arg );

	static void onTimer( void * arg );
	static void timer( void * arg );
};

#endif

