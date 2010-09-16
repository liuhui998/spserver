/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>

#include <gnutls/gnutls.h>
#include <gcrypt.h>
#include <errno.h>
#include <pthread.h>

GCRY_THREAD_OPTION_PTHREAD_IMPL;

int sp_init_gcrypt_pthread()
{
	return gcry_control( GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread );
}

