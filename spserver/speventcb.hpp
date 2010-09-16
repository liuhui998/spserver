/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __speventcb_hpp__
#define __speventcb_hpp__

class SP_HandlerFactory;
class SP_SessionManager;
class SP_Session;
class SP_BlockingQueue;
class SP_Message;
class SP_IOChannelFactory;

struct event_base;
typedef struct tagSP_Sid SP_Sid_t;

class SP_EventArg {
public:
	SP_EventArg( int timeout );
	~SP_EventArg();

	struct event_base * getEventBase() const;
	void * getResponseQueue() const;
	SP_BlockingQueue * getInputResultQueue() const;
	SP_BlockingQueue * getOutputResultQueue() const;
	SP_SessionManager * getSessionManager() const;

	void setTimeout( int timeout );
	int getTimeout() const;

private:
	struct event_base * mEventBase;
	void * mResponseQueue;

	SP_BlockingQueue * mInputResultQueue;
	SP_BlockingQueue * mOutputResultQueue;

	SP_SessionManager * mSessionManager;

	int mTimeout;
};

typedef struct tagSP_AcceptArg {
	SP_EventArg * mEventArg;

	SP_HandlerFactory * mHandlerFactory;
	SP_IOChannelFactory * mIOChannelFactory;
	int mReqQueueSize;
	int mMaxConnections;
	char * mRefusedMsg;
} SP_AcceptArg_t;

class SP_EventCallback {
public:
	static void onAccept( int fd, short events, void * arg );
	static void onRead( int fd, short events, void * arg );
	static void onWrite( int fd, short events, void * arg );

	static void onResponse( void * queueData, void * arg );

	static void addEvent( SP_Session * session, short events, int fd );

private:
	SP_EventCallback();
	~SP_EventCallback();
};

class SP_EventHelper {
public:
	static void doStart( SP_Session * session );
	static void start( void * arg );

	static void doWork( SP_Session * session );
	static void worker( void * arg );

	static void doError( SP_Session * session );
	static void error( void * arg );

	static void doTimeout( SP_Session * session );
	static void timeout( void * arg );

	static void doClose( SP_Session * session );
	static void myclose( void * arg );

	static void doCompletion( SP_EventArg * eventArg, SP_Message * msg );

	static int isSystemSid( SP_Sid_t * sid );

private:
	SP_EventHelper();
	~SP_EventHelper();
};

#endif

