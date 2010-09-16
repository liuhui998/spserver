/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */


#ifndef __spbuffer_hpp__
#define __spbuffer_hpp__

#include <stdlib.h>

#ifdef WIN32
typedef struct spwin32buffer sp_evbuffer_t;
#else
typedef struct evbuffer sp_evbuffer_t;
#endif

struct evbuffer;

class SP_Buffer {
public:
	SP_Buffer();
	~SP_Buffer();

	int append( const void * buffer, int len = 0 );
	int append( const SP_Buffer * buffer );
	int printf( const char *fmt, ... );

	void erase( int len );
	void reset();
	int truncate( int len );
	void reserve( int len );
	int getCapacity();

	const void * getBuffer() const;
	const void * getRawBuffer() const;
	size_t getSize() const;
	int take( char * buffer, int len );

	char * getLine();
	const void * find( const void * key, size_t len );

	SP_Buffer * take();

private:
	sp_evbuffer_t * mBuffer;

	friend class SP_IOChannel;
	friend class SP_IocpEventCallback;
};

#endif

