/*
 * Copyright 2007-2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "spporting.hpp"

#include "spsession.hpp"
#include "sphandler.hpp"
#include "spbuffer.hpp"
#include "sputils.hpp"
#include "sprequest.hpp"
#include "spiochannel.hpp"

#ifndef WIN32
#include "event.h"
#endif

//-------------------------------------------------------------------

typedef struct tagSP_SessionEntry {
	uint16_t mSeq;
	uint16_t mNext;

	SP_Session * mSession;
} SP_SessionEntry;

SP_SessionManager :: SP_SessionManager()
{
	mFreeCount = 0;
	mFreeList = 0;
	mCount = 0;
	memset( mArray, 0, sizeof( mArray ) );
}

SP_SessionManager :: ~SP_SessionManager()
{
	for( int i = 0; i < (int)( sizeof( mArray ) / sizeof( mArray[0] ) ); i++ ) {
		SP_SessionEntry_t * list = mArray[ i ];
		if( NULL != list ) {
			SP_SessionEntry_t * iter = list;
			for( int i = 0; i < eColPerRow; i++, iter++ ) {
				if( NULL != iter->mSession ) {
					delete iter->mSession;
					iter->mSession = NULL;
				}
			}
			free( list );
		}
	}

	memset( mArray, 0, sizeof( mArray ) );
}

uint16_t SP_SessionManager :: allocKey( uint16_t * seq )
{
	uint16_t key = 0;

	if( mFreeList <= 0 ) {
		int avail = -1;
		for( int i = 1; i < (int)( sizeof( mArray ) / sizeof( mArray[0] ) ); i++ ) {
			if( NULL == mArray[i] ) {
				avail = i;
				break;
			}
		}

		if( avail > 0 ) {
			mFreeCount += eColPerRow;
			mArray[ avail ] = ( SP_SessionEntry_t * )calloc(
					eColPerRow, sizeof( SP_SessionEntry_t ) );
			for( int i = eColPerRow - 1; i >= 0; i-- ) {
				mArray[ avail ] [ i ].mNext = mFreeList;
				mFreeList = eColPerRow * avail + i;
			}
		}
	}

	if( mFreeList > 0 ) {
		key = mFreeList;
		--mFreeCount;

		int row = mFreeList / eColPerRow, col = mFreeList % eColPerRow;

		*seq = mArray[ row ] [ col ].mSeq;
		mFreeList = mArray[ row ] [ col ].mNext;
	}

	return key;
}

int SP_SessionManager :: getCount()
{
	return mCount;
}

int SP_SessionManager :: getFreeCount()
{
	return mFreeCount;
}

void SP_SessionManager :: put( uint16_t key, uint16_t seq, SP_Session * session )
{
	int row = key / eColPerRow, col = key % eColPerRow;

	assert( NULL != mArray[ row ] );

	SP_SessionEntry_t * list = mArray[ row ];

	assert( NULL == list[ col ].mSession );
	assert( seq == list[ col ].mSeq );

	list[ col ].mSession = session;

	mCount++;
}

SP_Session * SP_SessionManager :: get( uint16_t key, uint16_t * seq )
{
	int row = key / eColPerRow, col = key % eColPerRow;

	SP_Session * ret = NULL;

	SP_SessionEntry_t * list = mArray[ row ];
	if( NULL != list ) {
		ret = list[ col ].mSession;
		* seq = list[ col ].mSeq;
	} else {
		* seq = 0;
	}

	return ret;
}

SP_Session * SP_SessionManager :: remove( uint16_t key, uint16_t seq )
{
	int row = key / eColPerRow, col = key % eColPerRow;

	SP_Session * ret = NULL;

	SP_SessionEntry_t * list = mArray[ row ];
	if( NULL != list ) {
		assert( seq == list[ col ].mSeq );

		ret = list[ col ].mSession;

		list[ col ].mSession = NULL;
		list[ col ].mSeq++;

		list[ col ].mNext = mFreeList;
		mFreeList = key;
		++mFreeCount;

		mCount--;
	}

	return ret;
}

//-------------------------------------------------------------------

SP_Session :: SP_Session( SP_Sid_t sid )
{
	mSid = sid;

	mReadEvent = NULL;
	mWriteEvent = NULL;

#ifndef WIN32
	mReadEvent = (struct event*)malloc( sizeof( struct event ) );
	mWriteEvent = (struct event*)malloc( sizeof( struct event ) );
#endif

	mHandler = NULL;
	mArg = NULL;

	mInBuffer = new SP_Buffer();
	mRequest = new SP_Request();

	mOutOffset = 0;
	mOutList = new SP_ArrayList();

	mStatus = eNormal;
	mRunning = 0;
	mWriting = 0;
	mReading = 0;

	mTotalRead = mTotalWrite = 0;

	mIOChannel = NULL;
}

SP_Session :: ~SP_Session()
{
	if( NULL != mReadEvent ) free( mReadEvent );
	mReadEvent = NULL;

	if( NULL != mWriteEvent ) free( mWriteEvent );
	mWriteEvent = NULL;

	if( NULL != mHandler ) {
		delete mHandler;
		mHandler = NULL;
	}

	delete mRequest;
	mRequest = NULL;

	delete mInBuffer;
	mInBuffer = NULL;

	delete mOutList;
	mOutList = NULL;

	if( NULL != mIOChannel ) {
		delete mIOChannel;
		mIOChannel = NULL;
	}
}

struct event * SP_Session :: getReadEvent()
{
	return mReadEvent;
}

struct event * SP_Session :: getWriteEvent()
{
	return mWriteEvent;
}

void SP_Session :: setHandler( SP_Handler * handler )
{
	mHandler = handler;
}

SP_Handler * SP_Session :: getHandler()
{
	return mHandler;
}

void SP_Session :: setArg( void * arg )
{
	mArg = arg;
}

void * SP_Session :: getArg()
{
	return mArg;
}

SP_Sid_t SP_Session :: getSid()
{
	return mSid;
}

SP_Buffer * SP_Session :: getInBuffer()
{
	return mInBuffer;
}

SP_Request * SP_Session :: getRequest()
{
	return mRequest;
}

void SP_Session :: setOutOffset( int offset )
{
	mOutOffset = offset;
}

int SP_Session :: getOutOffset()
{
	return mOutOffset;
}

SP_ArrayList * SP_Session :: getOutList()
{
	return mOutList;
}

void SP_Session :: setStatus( int status )
{
	mStatus = status;
}

int SP_Session :: getStatus()
{
	return mStatus;
}

int SP_Session :: getRunning()
{
	return mRunning;
}

void SP_Session :: setRunning( int running )
{
	mRunning = running;
}

int SP_Session :: getWriting()
{
	return mWriting;
}

void SP_Session :: setWriting( int writing )
{
	mWriting = writing;
}

int SP_Session :: getReading()
{
	return mReading;
}

void SP_Session :: setReading( int reading )
{
	mReading = reading;
}

SP_IOChannel * SP_Session :: getIOChannel()
{
	return mIOChannel;
}

void SP_Session :: setIOChannel( SP_IOChannel * ioChannel )
{
	mIOChannel = ioChannel;
}

unsigned int SP_Session :: getTotalRead()
{
	return mTotalRead;
}

void SP_Session :: addRead( int len )
{
	mTotalRead += len;
}

unsigned int SP_Session :: getTotalWrite()
{
	return mTotalWrite;
}

void SP_Session :: addWrite( int len )
{
	mTotalWrite += len;
}
