/*
 * Copyright 2007-2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>

#include "spporting.hpp"

#include "spmatrixssl.hpp"
#include "spioutils.hpp"
#include "spsession.hpp"
#include "spbuffer.hpp"
#include "speventcb.hpp"
#include "sputils.hpp"
#include "spmsgblock.hpp"

#include "sslSocket.h"

SP_MatrixsslChannel :: SP_MatrixsslChannel( sslKeys_t * keys )
{
	mKeys = keys;
	mConn = NULL;
}

SP_MatrixsslChannel :: ~SP_MatrixsslChannel()
{
	if( NULL != mConn ) {
		sslFreeConnection( &mConn );
	}
}

int SP_MatrixsslChannel :: init( int fd )
{
	SP_IOUtils::setBlock( fd );

	int ret = sslAccept( &mConn, fd, mKeys, NULL, 0 );

	SP_IOUtils::setNonblock( fd );

	if( 0 != ret ) {
		sp_syslog( LOG_EMERG, "sslAccept fail" );
		return -1;
	}

	return 0;
}

int SP_MatrixsslChannel :: receive( SP_Session * session )
{
	char buffer[ 4096 ] = { 0 };

	int ret = sslRead( mConn, buffer, sizeof( buffer ), &errno );
	if( ret > 0 ) {
		session->getInBuffer()->append( buffer, ret );
	} else if( ret < 0 ) {
		sp_syslog( LOG_EMERG, "sslRead fail" );
	}

	return ret;
}

int SP_MatrixsslChannel :: write_vec( struct iovec * iovArray, int iovSize )
{
	int len = 0;

	for( int i = 0; i < iovSize; i++ ) {
		int ret = sslWrite( mConn, (char*)iovArray[i].iov_base, iovArray[i].iov_len, &errno );
		if( ret > 0 ) len += ret;
		if( ret != (int)iovArray[i].iov_len ) break;
	}

	return len;
}

//---------------------------------------------------------

SP_MatrixsslChannelFactory :: SP_MatrixsslChannelFactory()
{
	mKeys = NULL;
}

SP_MatrixsslChannelFactory :: ~SP_MatrixsslChannelFactory()
{
	matrixSslFreeKeys( mKeys );
	matrixSslClose();
}

SP_IOChannel * SP_MatrixsslChannelFactory :: create() const
{
	return new SP_MatrixsslChannel( mKeys );
}

int SP_MatrixsslChannelFactory :: init( const char * certFile, const char * keyFile )
{
	if( matrixSslOpen() < 0 ) {
		sp_syslog( LOG_WARNING, "matrixSslOpen failed" );
	}

	if( matrixSslReadKeys( &mKeys, certFile, keyFile, NULL, NULL ) < 0 ) {
		sp_syslog( LOG_WARNING, "Error reading or parsing %s or %s.", certFile, keyFile );
	}

	return 0;
}

