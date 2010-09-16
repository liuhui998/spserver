/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "sputils.hpp"

const int SP_ArrayList::LAST_INDEX = -1;

SP_ArrayList :: SP_ArrayList( int initCount )
{
	mMaxCount = initCount <= 0 ? 2 : initCount;
	mCount = 0;
	mFirst = (void**)malloc( sizeof( void * ) * mMaxCount );
}

SP_ArrayList :: ~SP_ArrayList()
{
	free( mFirst );
	mFirst = NULL;
}

int SP_ArrayList :: getCount() const
{
	return mCount;
}

int SP_ArrayList :: append( void * value )
{
	if( NULL == value ) return -1;

	if( mCount >= mMaxCount ) {
		mMaxCount = ( mMaxCount * 3 ) / 2 + 1;
		mFirst = (void**)realloc( mFirst, sizeof( void * ) * mMaxCount );
		assert( NULL != mFirst );
		memset( mFirst + mCount, 0, ( mMaxCount - mCount ) * sizeof( void * ) );
	}

	mFirst[ mCount++ ] = value;

	return 0;
}

void * SP_ArrayList :: takeItem( int index )
{
	void * ret = NULL;

	if( LAST_INDEX == index ) index = mCount -1;
	if( index < 0 || index >= mCount ) return ret;

	ret = mFirst[ index ];

	mCount--;

	if( ( index + 1 ) < mMaxCount ) {
		memmove( mFirst + index, mFirst + index + 1,
			( mMaxCount - index - 1 ) * sizeof( void * ) );
	} else {
		mFirst[ index ] = NULL;
	}

	return ret;
}

const void * SP_ArrayList :: getItem( int index ) const
{
	const void * ret = NULL;

	if( LAST_INDEX == index ) index = mCount - 1;
	if( index < 0 || index >= mCount ) return ret;

	ret = mFirst[ index ];

	return ret;
}

void SP_ArrayList :: clean()
{
	mCount = 0;
	memset( mFirst, 0, sizeof( void * ) * mMaxCount );
}

//-------------------------------------------------------------------

SP_CircleQueue :: SP_CircleQueue()
{
	mMaxCount = 8;
	mEntries = (void**)malloc( sizeof( void * ) * mMaxCount );

	mHead = mTail = mCount = 0;
}

SP_CircleQueue :: ~SP_CircleQueue()
{
	free( mEntries );
	mEntries = NULL;
}

void SP_CircleQueue :: push( void * item )
{
	if( mCount >= mMaxCount ) {
		mMaxCount = ( mMaxCount * 3 ) / 2 + 1;
		void ** newEntries = (void**)malloc( sizeof( void * ) * mMaxCount );

		unsigned int headLen = 0, tailLen = 0;
		if( mHead < mTail ) {
			headLen = mTail - mHead;
		} else {
			headLen = mCount - mTail;
			tailLen = mTail;
		}

		memcpy( newEntries, &( mEntries[ mHead ] ), sizeof( void * ) * headLen );
		if( tailLen ) {
			memcpy( &( newEntries[ headLen ] ), mEntries, sizeof( void * ) * tailLen );
		}

		mHead = 0;
		mTail = headLen + tailLen;

		free( mEntries );
		mEntries = newEntries;
	}

	mEntries[ mTail++ ] = item;
	mTail = mTail % mMaxCount;
	mCount++;
}

void * SP_CircleQueue :: pop()
{
	void * ret = NULL;

	if( mCount > 0 ) {
		ret = mEntries[ mHead++ ];
		mHead = mHead % mMaxCount;
		mCount--;
	}

	return ret;
}

void * SP_CircleQueue :: top()
{
	return mCount > 0 ? mEntries[ mHead ] : NULL;
}

int SP_CircleQueue :: getLength()
{
	return mCount;
}

//-------------------------------------------------------------------

SP_BlockingQueue :: SP_BlockingQueue()
{
	mQueue = new SP_CircleQueue();
	sp_thread_mutex_init( &mMutex, NULL );
	sp_thread_cond_init( &mCond, NULL );
}

SP_BlockingQueue :: ~SP_BlockingQueue()
{
	delete mQueue;
	sp_thread_mutex_destroy( &mMutex );
	sp_thread_cond_destroy( &mCond );
}

void SP_BlockingQueue :: push( void * item )
{
	sp_thread_mutex_lock( &mMutex );

	mQueue->push( item );

	sp_thread_cond_signal( &mCond );

	sp_thread_mutex_unlock( &mMutex );
}

void * SP_BlockingQueue :: pop()
{
	void * ret = NULL;

	sp_thread_mutex_lock( &mMutex );

	if( mQueue->getLength() == 0 ) {
		sp_thread_cond_wait( &mCond, &mMutex );
	}

	ret = mQueue->pop();

	sp_thread_mutex_unlock( &mMutex );

	return ret;
}

void * SP_BlockingQueue :: top()
{
	void * ret = NULL;

	sp_thread_mutex_lock( &mMutex );

	ret = mQueue->top();

	sp_thread_mutex_unlock( &mMutex );

	return ret;
}

int SP_BlockingQueue :: getLength()
{
	int len = 0;

	sp_thread_mutex_lock( &mMutex );

	len = mQueue->getLength();

	sp_thread_mutex_unlock( &mMutex );

	return len;
}

//-------------------------------------------------------------------

int sp_strtok( const char * src, int index, char * dest, int len,
		char delimiter, const char ** next )
{
	int ret = 0;

	const char * pos1 = src, * pos2 = NULL;

	if( isspace( delimiter ) ) delimiter = 0;

	for( ; isspace( *pos1 ); ) pos1++;

	for ( int i = 0; i < index; i++ ) {
		if( 0 == delimiter ) {
			for( ; '\0' != *pos1 && !isspace( *pos1 ); ) pos1++;
			if( '\0' == *pos1 ) pos1 = NULL;
		} else {
			pos1 = strchr ( pos1, delimiter );
		}
		if ( NULL == pos1 ) break;

		pos1++;
		for( ; isspace( *pos1 ); ) pos1++;
	}

	*dest = '\0';
	if( NULL != next ) *next = NULL;

	if ( NULL != pos1 && '\0' != * pos1 ) {
		if( 0 == delimiter ) {
			for( pos2 = pos1; '\0' != *pos2 && !isspace( *pos2 ); ) pos2++;
			if( '\0' == *pos2 ) pos2 = NULL;
		} else {
			pos2 = strchr ( pos1, delimiter );
		}
		if ( NULL == pos2 ) {
			strncpy ( dest, pos1, len );
			if ( ((int)strlen(pos1)) >= len ) ret = -2;
		} else {
			if( pos2 - pos1 >= len ) ret = -2;
			len = ( pos2 - pos1 + 1 ) > len ? len : ( pos2 - pos1 + 1 );
			strncpy( dest, pos1, len );

			for( pos2++; isspace( *pos2 ); ) pos2++;
			if( NULL != next && '\0' != *pos2 ) *next = pos2;
		}
	} else {
		ret = -1;
	}

	dest[ len - 1 ] = '\0';
	len = strlen( dest );

	// remove tailing space
	for( ; len > 0 && isspace( dest[ len - 1 ] ); ) len--;
	dest[ len ] = '\0';

	return ret;
}

char * sp_strlcpy( char * dest, const char * src, int n )
{
	strncpy( dest, src, n );
	dest[ n - 1 ] = '\0';

	return dest;
}

