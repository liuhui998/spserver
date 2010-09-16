/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spmsgblock_hpp__
#define __spmsgblock_hpp__

#include <stdio.h>

class SP_Buffer;
class SP_ArrayList;

class SP_MsgBlock {
public:
	virtual ~SP_MsgBlock();

	virtual const void * getData() const = 0;
	virtual size_t getSize() const = 0;
};

class SP_MsgBlockList {
public:
	SP_MsgBlockList();
	~SP_MsgBlockList();

	void reset();

	size_t getTotalSize() const;

	int getCount() const;
	int append( SP_MsgBlock * msgBlock );
	const SP_MsgBlock * getItem( int index ) const;
	SP_MsgBlock * takeItem( int index );

private:
	SP_MsgBlockList( SP_MsgBlockList & );
	SP_MsgBlockList & operator=( SP_MsgBlockList & );

	SP_ArrayList * mList;
};

class SP_BufferMsgBlock : public SP_MsgBlock {
public:
	SP_BufferMsgBlock();
	SP_BufferMsgBlock( SP_Buffer * buffer, int toBeOwner );
	virtual ~SP_BufferMsgBlock();

	virtual const void * getData() const;
	virtual size_t getSize() const;

	int append( const void * buffer, size_t len = 0 );

private:
	SP_BufferMsgBlock( SP_BufferMsgBlock & );
	SP_BufferMsgBlock & operator=( SP_BufferMsgBlock & );

	SP_Buffer * mBuffer;
	int mToBeOwner;
};

class SP_SimpleMsgBlock : public SP_MsgBlock {
public:
	SP_SimpleMsgBlock();
	SP_SimpleMsgBlock( void * data, size_t size, int toBeOwner );
	virtual ~SP_SimpleMsgBlock();

	virtual const void * getData() const;
	virtual size_t getSize() const;

	void setData( void * data, size_t size, int toBeOwner );

private:
	SP_SimpleMsgBlock( SP_SimpleMsgBlock & );
	SP_SimpleMsgBlock & operator=( SP_SimpleMsgBlock & );

	void * mData;
	size_t mSize;
	int mToBeOwner;
};

#endif

