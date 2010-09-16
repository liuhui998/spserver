/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spmatrixssl_hpp__
#define __spmatrixssl_hpp__

#include "spiochannel.hpp"

typedef struct tagSslConn sslConn_t;

typedef int int32;
typedef int32 sslKeys_t;

class SP_MatrixsslChannel : public SP_IOChannel {
public:
	SP_MatrixsslChannel( sslKeys_t * keys );
	virtual ~SP_MatrixsslChannel();

	virtual int init( int fd );

	virtual int receive( SP_Session * session );

private:
	virtual int write_vec( struct iovec * iovArray, int iovSize );

	sslKeys_t * mKeys;
	sslConn_t * mConn;
};

class SP_MatrixsslChannelFactory : public SP_IOChannelFactory {
public:
	SP_MatrixsslChannelFactory();
	virtual ~SP_MatrixsslChannelFactory();

	virtual SP_IOChannel * create() const;

	int init( const char * certFile, const char * keyFile );

private:
	sslKeys_t * mKeys;
};

#endif

