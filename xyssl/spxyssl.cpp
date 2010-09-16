/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>

#include  "spporting.hpp"

/* xassl includes */
#include <xyssl/havege.h>
#include <xyssl/certs.h>
#include <xyssl/x509.h>
#include <xyssl/ssl.h>
#include <xyssl/net.h>

#include "spxyssl.hpp"

#include "spsession.hpp"
#include "spbuffer.hpp"
#include "speventcb.hpp"
#include "sputils.hpp"
#include "spmsgblock.hpp"
#include "spioutils.hpp"

/*
 * sorted by order of preference
 */
int SP_XysslChannel::xrly_ciphers[] =
{
#if !defined(NO_AES)
	SSL_RSA_AES_256_SHA,
#endif
#if !defined(NO_DES)
	SSL_RSA_DES_168_SHA,
#endif
#if !defined(NO_ARC4)
	SSL_RSA_RC4_128_SHA,
	SSL_RSA_RC4_128_MD5,
#endif
    0
};


SP_XysslChannel :: SP_XysslChannel( void * cert, void * key )
{
	mCert = cert;
	mKey = key;
	mCtx = NULL;
	mSession  = NULL;

	mFd = -1;
}

SP_XysslChannel :: ~SP_XysslChannel()
{
	if( NULL != mCtx ) {
		ssl_free( (ssl_context*)mCtx );
		free( mCtx );
	}

	mCtx = NULL;

	if( NULL != mSession ) free( mSession );
	mSession = NULL;
}

int SP_XysslChannel :: init( int fd )
{
	mCtx = malloc( sizeof( ssl_context ) );
	if( NULL == mCtx ) {
		sp_syslog( LOG_EMERG, "out of memory" );
		return -1;
	}

	ssl_context * ssl = (ssl_context*)mCtx;

	int ret = ssl_init( ssl );

	if( 0 != ret ) {
		sp_syslog( LOG_EMERG, "ssl_init failed: %08x", ret );
		return -1;
	}

	ssl_set_endpoint( ssl, SSL_IS_SERVER );

	/* FIXME: verify hook for client connections. */
	ssl_set_authmode( ssl, SSL_VERIFY_NONE );

	/* random number generation */
	havege_state hs;
	havege_init( &hs );
	ssl_set_rng( ssl, havege_rand, &hs );

	/* io */
	mFd = fd;
	ssl_set_bio( ssl, net_recv, &mFd, net_send, &mFd );

	/* ciphers */
	ssl_set_ciphers( ssl, xrly_ciphers );

	mSession = malloc( sizeof( ssl_session ) );
	memset( mSession, 0, sizeof( ssl_session ) );
	ssl_set_session( ssl, 1, 0, (ssl_session*)mSession );

	ssl_set_ca_chain( ssl, ((x509_cert*)mCert)->next, NULL );
	ssl_set_own_cert( ssl, (x509_cert*)mCert, (rsa_context*)mKey );

	while( ( ret = ssl_handshake( ssl ) ) != 0 ) {
		if( ret != XYSSL_ERR_NET_TRY_AGAIN ) {
			sp_syslog( LOG_EMERG, "ssl_handshake failed: %08x", ret );
			return -1;
		}
	}

	return 0;
}

int SP_XysslChannel :: receive( SP_Session * session )
{
	unsigned char buffer[ 4096 ] = { 0 };

	int ret = ssl_read( (ssl_context*)mCtx, buffer, sizeof( buffer ) );
	if( ret > 0 ) {
		session->getInBuffer()->append( buffer, ret );
	} else {
		if( XYSSL_ERR_NET_CONN_RESET == ret ) {
			ret = 0;
		} else {
			ret = -1;
			sp_syslog( LOG_EMERG, "ssl_read failed: %08x", ret );
		}
	}

	return ret;
}

int SP_XysslChannel :: write_vec( struct iovec * iovArray, int iovSize )
{
	int len = 0, ret = 0;
	for( int i = 0; i < iovSize; i++ ) {
		ret = ssl_write( (ssl_context*)mCtx, (unsigned char*)iovArray[i].iov_base, iovArray[i].iov_len );
		if( ret > 0 ) len += ret;
		if( ret != (int)iovArray[i].iov_len ) break;
	}

	//ssl_flush_output( (ssl_context*)mCtx );

	return len;
}

//---------------------------------------------------------

SP_XysslChannelFactory :: SP_XysslChannelFactory()
{
	mCert = NULL;
	mKey = NULL;
}

SP_XysslChannelFactory :: ~SP_XysslChannelFactory()
{
	if( NULL != mCert ) {
		x509_free( (x509_cert*)mCert );
		free( mCert );
	}

	mCert = NULL;

	if( NULL != mKey ) {
		rsa_free( (rsa_context*)mKey );
		free( mKey );
	}

	mKey = NULL;
}

SP_IOChannel * SP_XysslChannelFactory :: create() const
{
	return new SP_XysslChannel( mCert, mKey );
}

int SP_XysslChannelFactory :: init( const char * certFile, const char * keyFile )
{
	int ret = 0;

	mCert = malloc( sizeof( x509_cert ) );
	mKey = malloc( sizeof( rsa_context ) );

	memset( mCert, 0, sizeof( x509_cert ) );
	memset( mKey, 0, sizeof( rsa_context ) );

	x509_cert * cert = (x509_cert*)mCert;
	rsa_context * key = (rsa_context*)mKey;

	ret = x509parse_keyfile( key, (char*)keyFile, NULL );
	if( 0 != ret ) {
		sp_syslog( LOG_EMERG, "x509parse_keyfile failed: %08x", ret );
		return -1;
	}

	ret = x509parse_crtfile( cert, (char*)certFile );
	if( 0 != ret ) {
		sp_syslog( LOG_EMERG, "x509parse_crtfile failed: %08x", ret );
		return -1;
	}

	return ret;
}

