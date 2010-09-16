/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spgnutls_hpp__
#define __spgnutls_hpp__

#include "spiochannel.hpp"

struct gnutls_session_int;
struct gnutls_certificate_credentials_st;

class SP_GnutlsChannel : public SP_IOChannel {
public:
	SP_GnutlsChannel( struct gnutls_certificate_credentials_st * cred );
	virtual ~SP_GnutlsChannel();

	virtual int init( int fd );

	virtual int receive( SP_Session * session );

private:
	virtual int write_vec( struct iovec * vector, int count );

	gnutls_session_int * mTls;
	gnutls_certificate_credentials_st * mCred;
};

class SP_GnutlsChannelFactory : public SP_IOChannelFactory {
public:

	enum { DH_BITS = 1024 };

	SP_GnutlsChannelFactory();
	virtual ~SP_GnutlsChannelFactory();

	virtual SP_IOChannel * create() const;

	int init( const char * certFile, const char * keyFile );

private:
	gnutls_certificate_credentials_st * mCred;
};

#endif

