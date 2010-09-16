/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */


#ifndef __spsession_hpp__
#define __spsession_hpp__

#include "spresponse.hpp"

class SP_Handler;
class SP_Buffer;
class SP_Session;
class SP_ArrayList;
class SP_Request;
class SP_IOChannel;

struct event;

class SP_Session {
public:
	SP_Session( SP_Sid_t sid );
	virtual ~SP_Session();

	struct event * getReadEvent();	
	struct event * getWriteEvent();	

	void setHandler( SP_Handler * handler );
	SP_Handler * getHandler();

	void setArg( void * arg );
	void * getArg();

	SP_Sid_t getSid();

	SP_Buffer * getInBuffer();
	SP_Request * getRequest();

	void setOutOffset( int offset );
	int getOutOffset();
	SP_ArrayList * getOutList();

	enum { eNormal, eWouldExit, eExit };
	void setStatus( int status );
	int getStatus();

	int getRunning();
	void setRunning( int running );

	int getReading();
	void setReading( int reading );

	int getWriting();
	void setWriting( int writing );

	SP_IOChannel * getIOChannel();
	void setIOChannel( SP_IOChannel * ioChannel );

	unsigned int getTotalRead();
	void addRead( int len );

	unsigned int getTotalWrite();
	void addWrite( int len );

private:

	SP_Session( SP_Session & );
	SP_Session & operator=( SP_Session & );

	SP_Sid_t mSid;

	struct event * mReadEvent;
	struct event * mWriteEvent;

	SP_Handler * mHandler;
	void * mArg;

	SP_Buffer * mInBuffer;
	SP_Request * mRequest;

	int mOutOffset;
	SP_ArrayList * mOutList;

	char mStatus;
	char mRunning;
	char mWriting;
	char mReading;

	unsigned int mTotalRead, mTotalWrite;

	SP_IOChannel * mIOChannel;
};

typedef struct tagSP_SessionEntry SP_SessionEntry_t;

class SP_SessionManager {
public:
	SP_SessionManager();
	~SP_SessionManager();

	int getCount();
	void put( uint16_t key, uint16_t seq, SP_Session * session );
	SP_Session * get( uint16_t key, uint16_t * seq );
	SP_Session * remove( uint16_t key, uint16_t seq );

	int getFreeCount();
	// > 0 : OK, 0 : out of memory
	uint16_t allocKey( uint16_t * seq );

private:
	enum { eColPerRow = 1024 };

	int mCount;
	SP_SessionEntry_t * mArray[ 64 ];

	int mFreeCount;
	uint16_t mFreeList;
};

#endif

