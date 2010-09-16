/*
 * Copyright 2007-2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "spporting.hpp"

#include "sptunnelimpl.hpp"

#include "sprequest.hpp"
#include "spresponse.hpp"
#include "spbuffer.hpp"
#include "spmsgblock.hpp"

#ifdef WIN32
#include "spiocpdispatcher.hpp"
#else
#include "spdispatcher.hpp"
#endif

class SP_MutexGuard {
public:
	SP_MutexGuard( sp_thread_mutex_t * mutex ) {
		mMutex = mutex;
		sp_thread_mutex_lock( mMutex );
	}
	~SP_MutexGuard() { sp_thread_mutex_unlock( mMutex ); }
private:
	sp_thread_mutex_t * mMutex;
};

SP_TunnelArg * SP_TunnelArg :: create()
{
	return new SP_TunnelArg();
}

SP_TunnelArg :: SP_TunnelArg()
{
	sp_thread_mutex_init( &mMutex, NULL );
	mRefCount = 1;

	mTunnelStatus = mBackendStatus = eCreate;
	memset( &mTunnelSid, 0, sizeof( SP_Sid_t ) );
	memset( &mBackendSid, 0, sizeof( SP_Sid_t ) );
}

SP_TunnelArg :: ~SP_TunnelArg()
{
	sp_thread_mutex_destroy( &mMutex );
}

void SP_TunnelArg :: setTunnelStatus( int status )
{
	SP_MutexGuard gurad( &mMutex );

	mTunnelStatus = status;
}

int SP_TunnelArg :: getTunnelStatus()
{
	SP_MutexGuard gurad( &mMutex );

	return mTunnelStatus;
}

void SP_TunnelArg :: setTunnelSid( SP_Sid_t sid )
{
	SP_MutexGuard gurad( &mMutex );

	mTunnelSid = sid;
}

SP_Sid_t SP_TunnelArg :: getTunnelSid()
{
	SP_MutexGuard gurad( &mMutex );

	return mTunnelSid;
}

void SP_TunnelArg :: setBackendStatus( int status )
{
	SP_MutexGuard gurad( &mMutex );

	mBackendStatus = status;
}

int SP_TunnelArg :: getBackendStatus()
{
	SP_MutexGuard gurad( &mMutex );

	return mBackendStatus;
}

void SP_TunnelArg :: setBackendSid( SP_Sid_t sid )
{
	SP_MutexGuard gurad( &mMutex );

	mBackendSid = sid;
}

SP_Sid_t SP_TunnelArg :: getBackendSid()
{
	SP_MutexGuard gurad( &mMutex );

	return mBackendSid;
}

void SP_TunnelArg :: addRef()
{
	SP_MutexGuard gurad( &mMutex );

	mRefCount++;
}

void SP_TunnelArg :: release()
{
	int refCount = 1;

	sp_thread_mutex_lock( &mMutex );
	mRefCount--;
	refCount = mRefCount;
	sp_thread_mutex_unlock( &mMutex );

	if( refCount <= 0 ) delete this;
}

//---------------------------------------------------------

SP_TunnelDecoder :: SP_TunnelDecoder()
{
	mBuffer = NULL;
}

SP_TunnelDecoder :: ~SP_TunnelDecoder()
{
	if( NULL != mBuffer ) delete mBuffer;
	mBuffer = NULL;
}

int SP_TunnelDecoder :: decode( SP_Buffer * inBuffer )
{
	if( inBuffer->getSize() > 0 ) {
		if( NULL != mBuffer ) {
			mBuffer->append( inBuffer );
			inBuffer->reset();
		} else {
			mBuffer = inBuffer->take();
		}
	}

	int ret = SP_MsgDecoder::eMoreData;
	if( NULL != mBuffer && mBuffer->getSize() > 0 ) ret = SP_MsgDecoder::eOK;

	return ret;
}

SP_Buffer * SP_TunnelDecoder :: getBuffer()
{
	return mBuffer;
}

SP_Buffer * SP_TunnelDecoder :: takeBuffer()
{
	SP_Buffer * buffer = mBuffer;

	mBuffer = NULL;

	return buffer;
}

//---------------------------------------------------------

SP_BackendHandler :: SP_BackendHandler( SP_TunnelArg * tunnelArg )
{
	mArg = tunnelArg;
}

SP_BackendHandler :: ~SP_BackendHandler()
{
	mArg->release();
}

int SP_BackendHandler :: start( SP_Request * request, SP_Response * response )
{
	mArg->setBackendSid( response->getFromSid() );
	mArg->setBackendStatus( SP_TunnelArg::eNormal );

	request->setMsgDecoder( new SP_TunnelDecoder() );

	return 0;
}

int SP_BackendHandler :: handle( SP_Request * request, SP_Response * response )
{
	SP_TunnelDecoder * decoder = (SP_TunnelDecoder*)request->getMsgDecoder();
	SP_Buffer * buffer = decoder->takeBuffer();

	SP_Message * msg = new SP_Message();
	msg->getToList()->add( mArg->getTunnelSid() );
	msg->getFollowBlockList()->append( new SP_BufferMsgBlock( buffer, 1 ) );
	response->addMessage( msg );

	return SP_TunnelArg::eNormal == mArg->getTunnelStatus() ? 0 : -1;
}

void SP_BackendHandler :: error( SP_Response * response )
{
	mArg->setBackendStatus( SP_TunnelArg::eDestroy );
}

void SP_BackendHandler :: timeout( SP_Response * response )
{
	mArg->setBackendStatus( SP_TunnelArg::eDestroy );
}

void SP_BackendHandler :: close()
{
	mArg->setBackendStatus( SP_TunnelArg::eDestroy );
}

//---------------------------------------------------------

SP_TunnelHandler :: SP_TunnelHandler( SP_MyDispatcher * dispatcher,
		const char * dstHost, int dstPort )
{
	mDispatcher = dispatcher;
	mArg = SP_TunnelArg::create();

	mMsgBlockList = new SP_MsgBlockList();

	snprintf( mHost, sizeof( mHost ), "%s", dstHost );
	mPort = dstPort;
}

SP_TunnelHandler :: ~SP_TunnelHandler()
{
	mArg->release();
	mArg = NULL;

	delete mMsgBlockList;
	mMsgBlockList = NULL;
}

int SP_TunnelHandler :: start( SP_Request * request, SP_Response * response )
{
	mArg->setTunnelSid( response->getFromSid() );
	mArg->setTunnelStatus( SP_TunnelArg::eNormal );

	request->setMsgDecoder( new SP_TunnelDecoder() );

	int ret = 0;

	int socketFd = socket( AF_INET, SOCK_STREAM, IPPROTO_IP );
	if( socketFd >= 0 ) {
		struct sockaddr_in inAddr;
		inAddr.sin_family = AF_INET;
		inAddr.sin_addr.s_addr = inet_addr( mHost );
		inAddr.sin_port = htons( mPort );

		ret = connect( socketFd, (struct sockaddr*)&inAddr, sizeof( inAddr ) );
		if( 0 == ret ) {
			mArg->addRef();
			mDispatcher->push( socketFd, new SP_BackendHandler( mArg ) );
		} else {
			sp_syslog( LOG_WARNING, "Cannot connect to %s:%d", mHost, mPort );
			::close( socketFd );
		}
	} else {
		ret = -1;
		sp_syslog( LOG_WARNING, "Cannot open socket, errno %d, %s",
			errno, strerror( errno ) );
	}

	return ret;
}

int SP_TunnelHandler :: handle( SP_Request * request, SP_Response * response )
{
	SP_TunnelDecoder * decoder = (SP_TunnelDecoder*)request->getMsgDecoder();
	SP_Buffer * buffer = decoder->takeBuffer();

	if( SP_TunnelArg::eCreate == mArg->getBackendStatus() ) {
		mMsgBlockList->append( new SP_BufferMsgBlock( buffer, 1 ) );
	} else {
		SP_Message * msg = new SP_Message();
		msg->getToList()->add( mArg->getBackendSid() );

		for( ; mMsgBlockList->getCount() > 0; ) {
			msg->getFollowBlockList()->append( mMsgBlockList->takeItem( 0 ) );
		}

		msg->getFollowBlockList()->append( new SP_BufferMsgBlock( buffer, 1 ) );
		response->addMessage( msg );
	}

	return SP_TunnelArg::eDestroy != mArg->getBackendStatus() ? 0 : -1;
}

void SP_TunnelHandler :: error( SP_Response * response )
{
	mArg->setTunnelStatus( SP_TunnelArg::eDestroy );
}

void SP_TunnelHandler :: timeout( SP_Response * response )
{
	mArg->setTunnelStatus( SP_TunnelArg::eDestroy );
}

void SP_TunnelHandler :: close()
{
	mArg->setTunnelStatus( SP_TunnelArg::eDestroy );
}

