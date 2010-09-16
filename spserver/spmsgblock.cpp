/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include "spmsgblock.hpp"

#include "spbuffer.hpp"
#include "sputils.hpp"

SP_MsgBlock :: ~SP_MsgBlock()
{
}

//---------------------------------------------------------

SP_MsgBlockList :: SP_MsgBlockList()
{
	mList = new SP_ArrayList();
}

SP_MsgBlockList :: ~SP_MsgBlockList()
{
	for( int i = 0; i < mList->getCount(); i++ ) {
		SP_MsgBlock * msgBlock = (SP_MsgBlock*)mList->getItem( i );
		delete msgBlock;
	}
	delete mList;

	mList = NULL;
}

void SP_MsgBlockList :: reset()
{
	for( ; mList->getCount() > 0; ) {
		SP_MsgBlock * msgBlock = (SP_MsgBlock*)mList->takeItem( SP_ArrayList::LAST_INDEX );
		delete msgBlock;
	}
}

size_t SP_MsgBlockList :: getTotalSize() const
{
	size_t totalSize = 0;

	for( int i = 0; i < mList->getCount(); i++ ) {
		SP_MsgBlock * msgBlock = (SP_MsgBlock*)mList->getItem( i );
		totalSize += msgBlock->getSize();
	}

	return totalSize;
}

int SP_MsgBlockList :: getCount() const
{
	return mList->getCount();
}

int SP_MsgBlockList :: append( SP_MsgBlock * msgBlock )
{
	return mList->append( msgBlock );
}

const SP_MsgBlock * SP_MsgBlockList :: getItem( int index ) const
{
	return (SP_MsgBlock*)mList->getItem( index );
}

SP_MsgBlock * SP_MsgBlockList :: takeItem( int index )
{
	return (SP_MsgBlock*)mList->takeItem( index );
}

//---------------------------------------------------------

SP_BufferMsgBlock :: SP_BufferMsgBlock()
{
	mBuffer = new SP_Buffer();
	mToBeOwner = 1;
}

SP_BufferMsgBlock :: SP_BufferMsgBlock( SP_Buffer * buffer, int toBeOwner )
{
	mBuffer = buffer;
	mToBeOwner = toBeOwner;
}

SP_BufferMsgBlock :: ~SP_BufferMsgBlock()
{
	if( mToBeOwner ) delete mBuffer;
	mBuffer = NULL;
}

const void * SP_BufferMsgBlock :: getData() const
{
	return mBuffer->getBuffer();
}

size_t SP_BufferMsgBlock :: getSize() const
{
	return mBuffer->getSize();
}

int SP_BufferMsgBlock :: append( const void * buffer, size_t len )
{
	return mBuffer->append( buffer, len );
}

//---------------------------------------------------------

SP_SimpleMsgBlock :: SP_SimpleMsgBlock()
{
	mData = NULL;
	mSize = 0;
	mToBeOwner = 0;
}

SP_SimpleMsgBlock :: SP_SimpleMsgBlock( void * data, size_t size, int toBeOwner )
{
	mData = data;
	mSize = size;
	mToBeOwner = toBeOwner;
}

SP_SimpleMsgBlock :: ~SP_SimpleMsgBlock()
{
	if( mToBeOwner && NULL != mData ) {
		free( mData );
		mData = NULL;
	}
}

const void * SP_SimpleMsgBlock :: getData() const
{
	return mData;
}

size_t SP_SimpleMsgBlock :: getSize() const
{
	return mSize;
}

void SP_SimpleMsgBlock :: setData( void * data, size_t size, int toBeOwner )
{
	if( mToBeOwner && NULL != mData ) {
		free( mData );
	}

	mData = data;
	mSize = size;
	mToBeOwner = toBeOwner;
}

