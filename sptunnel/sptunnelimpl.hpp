/*
 * Copyright 2007-2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __sptunnelimpl_hpp__
#define __sptunnelimpl_hpp__

#include "spthread.hpp"

#include "spmsgdecoder.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"

class SP_TunnelArg {
public:
	static SP_TunnelArg * create();

	enum { eCreate, eNormal, eDestroy };

	void setTunnelStatus( int status );
	int getTunnelStatus();

	void setTunnelSid( SP_Sid_t sid );
	SP_Sid_t getTunnelSid();

	void setBackendStatus( int status );
	int getBackendStatus();

	void setBackendSid( SP_Sid_t sid );
	SP_Sid_t getBackendSid();

	void addRef();
	void release();

private:
	sp_thread_mutex_t mMutex;
	unsigned char mRefCount;

	unsigned char mTunnelStatus, mBackendStatus;
	SP_Sid_t mTunnelSid, mBackendSid;

	SP_TunnelArg();
	~SP_TunnelArg();
};

class SP_TunnelDecoder : public SP_MsgDecoder {
public:
	SP_TunnelDecoder();
	virtual ~SP_TunnelDecoder();

	virtual int decode( SP_Buffer * inBuffer );

	SP_Buffer * getBuffer();

	SP_Buffer * takeBuffer();

private:
	SP_Buffer * mBuffer;
};

class SP_BackendHandler : public SP_Handler {
public:
	SP_BackendHandler( SP_TunnelArg * tunnelArg );
	virtual ~SP_BackendHandler();

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response );

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response );

	virtual void error( SP_Response * response );

	virtual void timeout( SP_Response * response );

	virtual void close();

private:
	SP_TunnelArg * mArg;
};

#ifdef WIN32
typedef class SP_IocpDispatcher SP_MyDispatcher;
#else
typedef class SP_Dispatcher SP_MyDispatcher;
#endif

class SP_MsgBlockList;

class SP_TunnelHandler : public SP_Handler {
public:
	SP_TunnelHandler( SP_MyDispatcher * dispatcher,
			const char * dstHost, int dstPort );

	virtual ~SP_TunnelHandler();

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response );

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response );

	virtual void error( SP_Response * response );

	virtual void timeout( SP_Response * response );

	virtual void close();

private:
	SP_MyDispatcher * mDispatcher;
	SP_TunnelArg * mArg;

	SP_MsgBlockList * mMsgBlockList;

	char mHost[ 32 ];
	int mPort;
};

#endif

