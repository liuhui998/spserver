/*
 * Copyright 2007-2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>
#include <ctype.h>

#include "spporting.hpp"

#include "spgnutls.hpp"
#include "spsession.hpp"
#include "spbuffer.hpp"
#include "speventcb.hpp"
#include "sputils.hpp"
#include "spmsgblock.hpp"
#include "spioutils.hpp"

#ifdef WIN32
#ifndef HAVE_SSIZE_T
typedef long ssize_t;
#endif
#endif

#include <gnutls/gnutls.h>
#include "spgcrypt.h"

SP_GnutlsChannel :: SP_GnutlsChannel( struct gnutls_certificate_credentials_st * cred )
{
	mCred = cred;
	mTls = NULL;
}

SP_GnutlsChannel :: ~SP_GnutlsChannel()
{
	if( NULL != mTls )
	{
		/* do not wait for the peer to close the connection.
		 */
		gnutls_bye( mTls, GNUTLS_SHUT_WR );

		gnutls_deinit( mTls );
	}
	mTls = NULL;
}

int SP_GnutlsChannel :: init( int fd )
{
	gnutls_init( &mTls, GNUTLS_SERVER );

	/* avoid calling all the priority functions, since the defaults
	 * are adequate.
	 */
	gnutls_set_default_priority( mTls );

	gnutls_credentials_set( mTls, GNUTLS_CRD_CERTIFICATE, mCred );

	/* request client certificate if any.
	 */
	gnutls_certificate_server_set_request( mTls, GNUTLS_CERT_REQUEST );

	gnutls_dh_set_prime_bits( mTls, SP_GnutlsChannelFactory::DH_BITS );

	gnutls_transport_set_ptr( mTls, (gnutls_transport_ptr_t) fd );

	/* we run in an independence thread, and we can block when gnutls_handshake */

	SP_IOUtils::setBlock( fd );
	int ret = gnutls_handshake( mTls );
	if( ret < 0 ) {
		sp_syslog( LOG_EMERG, "gnutls_handshake fail, %s", gnutls_strerror( ret ) );
		return -1;
	}

	SP_IOUtils::setNonblock( fd );

	return 0;
}

int SP_GnutlsChannel :: receive( SP_Session * session )
{
	char buffer[ 4096 ] = { 0 };

	int ret = gnutls_record_recv( mTls, buffer, sizeof( buffer ) );
	if( ret > 0 ) {
		session->getInBuffer()->append( buffer, ret );
	} else if( ret < 0 ) {
		sp_syslog( LOG_EMERG, "gnutls_record_recv fail, %s", gnutls_strerror( ret ) );
	}

	return ret;
}

int SP_GnutlsChannel :: write_vec( struct iovec * iovArray, int iovSize )
{
	int len = 0;
	for( int i = 0; i < iovSize; i++ ) {
		int ret = gnutls_record_send( mTls, iovArray[i].iov_base, iovArray[i].iov_len );
		if( ret > 0 ) len += ret;
		if( ret != (int)iovArray[i].iov_len ) break;
	}

	return len;
}

//---------------------------------------------------------

SP_GnutlsChannelFactory :: SP_GnutlsChannelFactory()
{
	mCred = NULL;
}

SP_GnutlsChannelFactory :: ~SP_GnutlsChannelFactory()
{
	if( NULL != mCred )
	{
		gnutls_certificate_free_credentials( mCred );
		gnutls_global_deinit();
	}
	mCred = NULL;
}

SP_IOChannel * SP_GnutlsChannelFactory :: create() const
{
	return new SP_GnutlsChannel( mCred );
}

int SP_GnutlsChannelFactory :: init( const char * certFile, const char * keyFile )
{
#ifndef WIN32
	sp_init_gcrypt_pthread();
#endif

	int ret = 0;

	if( ( ret = gnutls_global_init() ) < 0 )
	{
		sp_syslog( LOG_ERR, "global_init: %s\n", gnutls_strerror( ret ) );
		return -1;
	}

	gnutls_certificate_allocate_credentials( &mCred );

	//gnutls_certificate_set_x509_trust_file( mCred, caFile, GNUTLS_X509_FMT_PEM );
	//gnutls_certificate_set_x509_crl_file( mCred, crlFile, GNUTLS_X509_FMT_PEM );

	gnutls_certificate_set_x509_key_file( mCred, certFile, keyFile, GNUTLS_X509_FMT_PEM );

	static gnutls_dh_params_t dh_params;

	/* Generate Diffie Hellman parameters - for use with DHE
	 * kx algorithms. These should be discarded and regenerated
	 * once a day, once a week or once a month. Depending on the
	 * security requirements.
	 */
	gnutls_dh_params_init( &dh_params );
	gnutls_dh_params_generate2( dh_params, DH_BITS );

	gnutls_certificate_set_dh_params( mCred, dh_params );

	return 0;
}

