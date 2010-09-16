/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spxyssl_hpp__
#define __spxyssl_hpp__

#include "spiochannel.hpp"

class SP_XysslChannel : public SP_IOChannel {
public:
	SP_XysslChannel( void * cert, void * key );
	virtual ~SP_XysslChannel();

	virtual int init( int fd );

	virtual int receive( SP_Session * session );

private:
	virtual int write_vec( struct iovec * vector, int count );

	int mFd;

	void * mCtx, * mSession;
	void * mCert, * mKey;

	static int xrly_ciphers[];
};

class SP_XysslChannelFactory : public SP_IOChannelFactory {
public:
	SP_XysslChannelFactory();
	virtual ~SP_XysslChannelFactory();

	virtual SP_IOChannel * create() const;

	int init( const char * certFile, const char * keyFile );

private:
	void * mCert;
	void * mKey;
};

#endif

