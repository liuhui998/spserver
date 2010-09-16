/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include "spporting.hpp"

#include "spbuffer.hpp"

#ifdef WIN32

#include "spwin32buffer.hpp"

#else

#include "event.h"

#define sp_evbuffer_new         evbuffer_new
#define sp_evbuffer_free        evbuffer_free
#define sp_evbuffer_add         evbuffer_add
#define sp_evbuffer_drain       evbuffer_drain
#define sp_evbuffer_expand      evbuffer_expand
#define sp_evbuffer_remove      evbuffer_remove
#define sp_evbuffer_readline    evbuffer_readline
#define sp_evbuffer_add_vprintf evbuffer_add_vprintf

#endif

SP_Buffer :: SP_Buffer()
{
	mBuffer = sp_evbuffer_new();
}

SP_Buffer :: ~SP_Buffer()
{
	sp_evbuffer_free( mBuffer );
	mBuffer = NULL;
}

int SP_Buffer :: append( const void * buffer, int len )
{
	len = len <= 0 ? strlen( (char*)buffer ) : len;

	return sp_evbuffer_add( mBuffer, (void*)buffer, len );
}

int SP_Buffer :: append( const SP_Buffer * buffer )
{
	if( buffer->getSize() > 0 ) {
		return append( buffer->getBuffer(), buffer->getSize() );
	} else {
		return 0;
	}
}

int SP_Buffer :: printf( const char *fmt, ... )
{
	int ret = 0;

	if( NULL != strchr( fmt, '%' ) ) {
		va_list args;
		va_start(args, fmt);
		ret = sp_evbuffer_add_vprintf( mBuffer, fmt, args );
		va_end(args);
	} else {
		ret = append( fmt );
	}

	return ret;
}

void SP_Buffer :: erase( int len )
{
	sp_evbuffer_drain( mBuffer, len );
}

void SP_Buffer :: reset()
{
	erase( getSize() );
}

int SP_Buffer :: truncate( int len )
{
	if( len < (int)getSize() ) {
		EVBUFFER_LENGTH( mBuffer ) = len;
		return 0;
	}

	return -1;
}

void SP_Buffer :: reserve( int len )
{
	sp_evbuffer_expand( mBuffer, len - getSize() );
}

int SP_Buffer :: getCapacity()
{
	return mBuffer->totallen;
}

const void * SP_Buffer :: getBuffer() const
{
	if( NULL != EVBUFFER_DATA( mBuffer ) ) {
		sp_evbuffer_expand( mBuffer, 1 );
		((char*)(EVBUFFER_DATA( mBuffer )))[ getSize() ] = '\0';
		return EVBUFFER_DATA( mBuffer );
	} else {
		return "";
	}
}

const void * SP_Buffer :: getRawBuffer() const
{
	return EVBUFFER_DATA( mBuffer );
}

size_t SP_Buffer :: getSize() const
{
	return EVBUFFER_LENGTH( mBuffer );
}

char * SP_Buffer :: getLine()
{
	return sp_evbuffer_readline( mBuffer );
}

int SP_Buffer :: take( char * buffer, int len )
{
	len = sp_evbuffer_remove( mBuffer, buffer, len - 1);
	buffer[ len ] = '\0';

	return len;
}

SP_Buffer * SP_Buffer :: take()
{
	SP_Buffer * ret = new SP_Buffer();

	sp_evbuffer_t * tmp = ret->mBuffer;
	ret->mBuffer = mBuffer;
	mBuffer = tmp;

	return ret;
}

const void * SP_Buffer :: find( const void * key, size_t len )
{
	//return (void*)evbuffer_find( mBuffer, (u_char*)key, len );

	sp_evbuffer_t * buffer = mBuffer;
	u_char * what = (u_char*)key;

	size_t remain = buffer->off;
	u_char *search = buffer->buffer;
	u_char *p;

	while (remain >= len) {
		if ((p = (u_char*)memchr(search, *what, (remain - len) + 1)) == NULL)
			break;

		if (memcmp(p, what, len) == 0)
			return (p);

		search = p + 1;
		remain = buffer->off - (size_t)(search - buffer->buffer);
	}

	return (NULL);
}

