/*
 * Copyright 2007-2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spopenssl_hpp__
#define __spopenssl_hpp__

#include "spiochannel.hpp"

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;

class SP_OpensslChannel : public SP_IOChannel {
public:
	SP_OpensslChannel( SSL_CTX * ctx );
	virtual ~SP_OpensslChannel();

	virtual int init( int fd );

	virtual int receive( SP_Session * session );

private:
	virtual int write_vec( struct iovec * vector, int count );

	SSL_CTX * mCtx;
	SSL * mSsl;
};

class SP_OpensslChannelFactory : public SP_IOChannelFactory {
public:
	SP_OpensslChannelFactory();
	virtual ~SP_OpensslChannelFactory();

	virtual SP_IOChannel * create() const;

	int init( const char * certFile, const char * keyFile );

private:
	SSL_CTX * mCtx;
};

#endif

