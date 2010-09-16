/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */


#ifndef __spresponse_hpp__
#define __spresponse_hpp__

#include <sys/types.h>
#include "spporting.hpp"

class SP_Buffer;
struct evbuffer;
class SP_ArrayList;
class SP_MsgBlockList;

typedef struct tagSP_Sid {
	uint16_t mKey;
	uint16_t mSeq;

	enum {
		eTimerKey = 0,
		eTimerSeq = 65535,

		ePushKey = 1,
		ePushSeq = 65535
	};
} SP_Sid_t;

class SP_SidList {
public:
	SP_SidList();
	~SP_SidList();

	void reset();

	int getCount() const;
	void add( SP_Sid_t sid );
	SP_Sid_t get( int index ) const;
	SP_Sid_t take( int index );

	int find( SP_Sid_t sid ) const;

private:
	SP_SidList( SP_SidList & );
	SP_SidList & operator=( SP_SidList & );

	SP_ArrayList * mList;
};

class SP_Message {
public:
	SP_Message( int completionKey = 0 );
	~SP_Message();

	void reset();

	SP_SidList * getToList();

	size_t getTotalSize();

	SP_Buffer * getMsg();
	SP_MsgBlockList * getFollowBlockList();

	SP_SidList * getSuccess();
	SP_SidList * getFailure();

	void setCompletionKey( int completionKey );
	int getCompletionKey();

private:
	SP_Message( SP_Message & );
	SP_Message & operator=( SP_Message & );

	SP_Buffer * mMsg;
	SP_MsgBlockList * mFollowBlockList;

	SP_SidList * mToList;
	SP_SidList * mSuccess;
	SP_SidList * mFailure;

	int mCompletionKey;
};

class SP_Response {
public:
	SP_Response( SP_Sid_t fromSid );
	~SP_Response();

	SP_Sid_t getFromSid() const;
	SP_Message * getReply();

	void addMessage( SP_Message * msg );
	SP_Message * peekMessage();
	SP_Message * takeMessage();

	SP_SidList * getToCloseList();

private:
	SP_Response( SP_Response & );
	SP_Response & operator=( SP_Response & );

	SP_Sid_t mFromSid;
	SP_Message * mReply;
	SP_SidList * mToCloseList;

	SP_ArrayList * mList;
};

#endif

