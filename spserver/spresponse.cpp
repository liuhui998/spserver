/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>

#include "spresponse.hpp"
#include "spbuffer.hpp"
#include "sputils.hpp"
#include "spmsgblock.hpp"

//-------------------------------------------------------------------

SP_SidList :: SP_SidList()
{
	mList = new SP_ArrayList();
}

SP_SidList :: ~SP_SidList()
{
	for( int i = 0; i < mList->getCount(); i++ ) {
		free( (void*)mList->getItem( i ) );
	}

	delete mList;
	mList = NULL;
}

void SP_SidList :: reset()
{
	for( ; mList->getCount() > 0; ) {
		free( (void*)mList->takeItem( SP_ArrayList::LAST_INDEX ) );
	}
}

int SP_SidList :: getCount() const
{
	return mList->getCount();
}

void SP_SidList :: add( SP_Sid_t sid )
{
	SP_Sid_t * p = (SP_Sid_t*)malloc( sizeof( SP_Sid_t ) );
	*p = sid;

	mList->append( p );
}

SP_Sid_t SP_SidList :: get( int index ) const
{
	SP_Sid_t ret = { 0, 0 };

	SP_Sid_t * p = (SP_Sid_t*)mList->getItem( index );
	if( NULL != p ) ret = *p;

	return ret;
}

SP_Sid_t SP_SidList :: take( int index )
{
	SP_Sid_t ret = get( index );

	void * p = mList->takeItem( index );
	if( NULL != p ) free( p );

	return ret;
}

int SP_SidList :: find( SP_Sid_t sid ) const
{
	for( int i = 0; i < mList->getCount(); i++ ) {
		SP_Sid_t * p = (SP_Sid_t*)mList->getItem( i );

		if( p->mKey == sid.mKey && p->mSeq == sid.mSeq ) return i;
	}

	return -1;
}

//-------------------------------------------------------------------

SP_Message :: SP_Message( int completionKey )
{
	mCompletionKey = completionKey;

	mMsg = NULL;
	mFollowBlockList = NULL;

	mToList = mSuccess = mFailure = NULL;
}

SP_Message :: ~SP_Message()
{
	if( NULL != mMsg ) delete mMsg;
	mMsg = NULL;

	if( NULL != mFollowBlockList ) delete mFollowBlockList;
	mFollowBlockList = NULL;

	if( NULL != mToList ) delete mToList;
	mToList = NULL;

	if( NULL != mSuccess ) delete mSuccess;
	mSuccess = NULL;

	if( NULL != mFailure ) delete mFailure;
	mFailure = NULL;
}

void SP_Message :: reset()
{
	if( NULL != mMsg ) mMsg->reset();

	if( NULL != mFollowBlockList ) mFollowBlockList->reset();

	if( NULL != mToList ) mToList->reset();

	if( NULL != mSuccess ) mSuccess->reset();

	if( NULL != mFailure ) mFailure->reset();
}

SP_SidList * SP_Message :: getToList()
{
	if( NULL == mToList ) mToList = new SP_SidList();

	return mToList;
}

size_t SP_Message :: getTotalSize()
{
	size_t totalSize = 0;

	if( NULL != mMsg ) totalSize += mMsg->getSize();
	if( NULL != mFollowBlockList ) totalSize += mFollowBlockList->getTotalSize();

	return totalSize;
}

SP_Buffer * SP_Message :: getMsg()
{
	if( NULL == mMsg ) mMsg = new SP_Buffer();

	return mMsg;
}

SP_MsgBlockList * SP_Message :: getFollowBlockList()
{
	if( NULL == mFollowBlockList ) mFollowBlockList = new SP_MsgBlockList();

	return mFollowBlockList;
}

SP_SidList * SP_Message :: getSuccess()
{
	if( NULL == mSuccess ) mSuccess = new SP_SidList();

	return mSuccess;
}

SP_SidList * SP_Message :: getFailure()
{
	if( NULL == mFailure ) mFailure = new SP_SidList();

	return mFailure;
}

void SP_Message :: setCompletionKey( int completionKey )
{
	mCompletionKey = completionKey;
}

int SP_Message :: getCompletionKey()
{
	return mCompletionKey;
}

//-------------------------------------------------------------------

SP_Response :: SP_Response( SP_Sid_t fromSid )
{
	mFromSid = fromSid;

	mReply = NULL;

	mList = new SP_ArrayList();

	mToCloseList = NULL;
}

SP_Response :: ~SP_Response()
{
	for( int i = 0; i < mList->getCount(); i++ ) {
		delete (SP_Message*)mList->getItem( i );
	}

	delete mList;
	mList = NULL;

	mReply = NULL;

	if( NULL != mToCloseList ) delete mToCloseList;
	mToCloseList = NULL;
}

SP_Sid_t SP_Response :: getFromSid() const
{
	return mFromSid;
}

SP_Message * SP_Response :: getReply()
{
	if( NULL == mReply ) {
		mReply = new SP_Message();
		mReply->getToList()->add( mFromSid );
		mList->append( mReply );
	}

	return mReply;
}

void SP_Response :: addMessage( SP_Message * msg )
{
	mList->append( msg );
}

SP_Message * SP_Response :: peekMessage()
{
	return ( SP_Message * ) mList->getItem( 0 );
}

SP_Message * SP_Response :: takeMessage()
{
	return ( SP_Message * ) mList->takeItem( 0 );
}

SP_SidList * SP_Response :: getToCloseList()
{
	if( NULL == mToCloseList ) mToCloseList = new SP_SidList();
	return mToCloseList;
}

