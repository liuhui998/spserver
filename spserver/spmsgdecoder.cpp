/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "spmsgdecoder.hpp"

#include "spbuffer.hpp"
#include "sputils.hpp"

//-------------------------------------------------------------------

SP_MsgDecoder :: ~SP_MsgDecoder()
{
}

//-------------------------------------------------------------------

SP_DefaultMsgDecoder :: SP_DefaultMsgDecoder()
{
	mBuffer = new SP_Buffer();
}

SP_DefaultMsgDecoder :: ~SP_DefaultMsgDecoder()
{
	if( NULL != mBuffer ) delete mBuffer;
	mBuffer = NULL;
}

int SP_DefaultMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	if( inBuffer->getSize() > 0 ) {
		mBuffer->reset();
		mBuffer->append( inBuffer );
		inBuffer->reset();

		return eOK;
	}

	return eMoreData;
}

SP_Buffer * SP_DefaultMsgDecoder :: getMsg()
{
	return mBuffer;
}

//-------------------------------------------------------------------

SP_LineMsgDecoder :: SP_LineMsgDecoder()
{
	mLine = NULL;
}

SP_LineMsgDecoder :: ~SP_LineMsgDecoder()
{
	if( NULL != mLine ) {
		free( mLine );
		mLine = NULL;
	}
}

int SP_LineMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	if( NULL != mLine ) free( mLine );
	mLine = inBuffer->getLine();

	return NULL == mLine ? eMoreData : eOK;
}

const char * SP_LineMsgDecoder :: getMsg()
{
	return mLine;
}

//-------------------------------------------------------------------

SP_MultiLineMsgDecoder :: SP_MultiLineMsgDecoder()
{
	mQueue = new SP_CircleQueue();
}

SP_MultiLineMsgDecoder :: ~SP_MultiLineMsgDecoder()
{
	for( ; NULL != mQueue->top(); ) {
		free( (void*)mQueue->pop() );
	}

	delete mQueue;
	mQueue = NULL;
}

int SP_MultiLineMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	int ret = eMoreData;

	for( ; ; ) {
		char * line = inBuffer->getLine();
		if( NULL == line ) break;
		mQueue->push( line );
		ret = eOK;
	}

	return ret;
}

SP_CircleQueue * SP_MultiLineMsgDecoder :: getQueue()
{
	return mQueue;
}

//-------------------------------------------------------------------

SP_DotTermMsgDecoder :: SP_DotTermMsgDecoder()
{
	mBuffer = NULL;
}

SP_DotTermMsgDecoder :: ~SP_DotTermMsgDecoder()
{
	if( NULL != mBuffer ) {
		free( mBuffer );
	}
	mBuffer = NULL;
}

int SP_DotTermMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	if( NULL != mBuffer ) {
		free( mBuffer );
		mBuffer = NULL;
	}

	const char * pos = (char*)inBuffer->find( "\r\n.\r\n", 5 );	

	if( NULL == pos ) {
		pos = (char*)inBuffer->find( "\n.\n", 3 );
	}

	if( NULL != pos ) {
		int len = pos - (char*)inBuffer->getRawBuffer();

		mBuffer = (char*)malloc( len + 1 );
		memcpy( mBuffer, inBuffer->getBuffer(), len );
		mBuffer[ len ] = '\0';

		inBuffer->erase( len );

		/* remove with the "\n.." */
		char * src, * des;
		for( src = des = mBuffer + 1; * src != '\0'; ) {
			if( '.' == *src && '\n' == * ( src - 1 ) ) src++ ;
			* des++ = * src++;
		}
		* des = '\0';

		if( 0 == strncmp( (char*)pos, "\n.\n", 3 ) ) {
			inBuffer->erase( 3 );
		} else  {
			inBuffer->erase( 5 );
		}
		return eOK;
	} else {
		return eMoreData;
	}
}

const char * SP_DotTermMsgDecoder :: getMsg()
{
	return mBuffer;
}

//-------------------------------------------------------------------

SP_DotTermChunkMsgDecoder :: SP_DotTermChunkMsgDecoder()
{
	mList = new SP_ArrayList();
}

SP_DotTermChunkMsgDecoder :: ~SP_DotTermChunkMsgDecoder()
{
	for( int i = 0; i < mList->getCount(); i++ ) {
		SP_Buffer * item = (SP_Buffer*)mList->getItem( i );
		delete item;
	}

	delete mList, mList = NULL;
}

int SP_DotTermChunkMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	if( inBuffer->getSize() <= 0 ) return eMoreData;

	const char * pos = (char*)inBuffer->find( "\r\n.\r\n", 5 );	

	if( NULL == pos ) {
		pos = (char*)inBuffer->find( "\n.\n", 3 );
	}

	if( NULL != pos ) {
		if( pos != inBuffer->getRawBuffer() ) {
			int len = pos - (char*)inBuffer->getRawBuffer();

			SP_Buffer * last = new SP_Buffer();
			last->append( inBuffer->getBuffer(), len );
			mList->append( last );

			inBuffer->erase( len );
		}

		if( 0 == strncmp( (char*)pos, "\n.\n", 3 ) ) {
			inBuffer->erase( 3 );
		} else  {
			inBuffer->erase( 5 );
		}

		return eOK;
	} else {
		if( mList->getCount() > 0 ) {
			char dotTerm[ 16 ] = { 0 };

			SP_Buffer * prevBuffer = (SP_Buffer*)mList->getItem( SP_ArrayList::LAST_INDEX );
			pos = ((char*)prevBuffer->getRawBuffer()) + prevBuffer->getSize() - 5;
			memcpy( dotTerm, pos, 5 );

			if( inBuffer->getSize() > 5 ) {
				memcpy( dotTerm + 5, inBuffer->getRawBuffer(), 5 );
			} else {
				memcpy( dotTerm + 5, inBuffer->getRawBuffer(), inBuffer->getSize() );
			}

			pos = strstr( dotTerm, "\r\n.\r\n" );
			if( NULL == pos ) pos = strstr( dotTerm, "\n.\n" );

			if( NULL != pos ) {
				int prevLen = 5 - ( pos - dotTerm );
				int lastLen = 5 - prevLen;
				if( 0 == strncmp( (char*)pos, "\n.\n", 3 )) lastLen = 3 - prevLen;

				assert( prevLen < 5 && lastLen < 5 );

				prevBuffer->truncate( prevBuffer->getSize() - prevLen );
				inBuffer->erase( lastLen );

				return eOK;
			}
		}

		if( inBuffer->getSize() >= ( MAX_SIZE_PER_CHUNK - 1024 * 4 ) ) {
			mList->append( inBuffer->take() );
		}
		inBuffer->reserve( MAX_SIZE_PER_CHUNK );
	}

	return eMoreData;
}

char * SP_DotTermChunkMsgDecoder :: getMsg()
{
	int i = 0, totalSize = 0;
	for( i = 0; i < mList->getCount(); i++ ) {
		SP_Buffer * item = (SP_Buffer*)mList->getItem( i );
		totalSize += item->getSize();
	}

	char * ret = (char*)malloc( totalSize + 1 );
	ret[ totalSize ] = '\0';

	char * des = ret, * src = NULL;

	for( i = 0; i < mList->getCount(); i++ ) {
		SP_Buffer * item = (SP_Buffer*)mList->getItem( i );
		memcpy( des, item->getRawBuffer(), item->getSize() );
		des += item->getSize();
	}

	for( i = 0; i < mList->getCount(); i++ ) {
		SP_Buffer * item = (SP_Buffer*)mList->getItem( i );
		delete item;
	}
	mList->clean();

	/* remove with the "\n.." */
	for( src = des = ret + 1; * src != '\0'; ) {
		if( '.' == *src && '\n' == * ( src - 1 ) ) src++ ;
		* des++ = * src++;
	}
	* des = '\0';

	return ret;
}

