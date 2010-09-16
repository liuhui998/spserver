/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#include "spthread.hpp"

#include "spporting.hpp"

#include "spthreadpool.hpp"

extern int errno;

void threadFunc( void *arg )
{
	int seconds = (int) arg;

	fprintf( stdout, "  in threadFunc %d\n", seconds );
	fprintf( stdout, "  thread#%ld\n", sp_thread_self() );
	sleep( seconds );
	fprintf( stdout, "  done threadFunc %d\n", seconds);
}

int main( int argc, char ** argv )
{
	SP_ThreadPool pool( 2 );

	fprintf( stdout, "**main** dispatch 3\n" );
	pool.dispatch( threadFunc, (void*)3 );
	fprintf( stdout, "**main** dispatch 6\n");
	pool.dispatch( threadFunc, (void*)6 );
	fprintf( stdout, "**main** dispatch 7\n");
	pool.dispatch( threadFunc, (void*)7 );

	fprintf( stdout, "**main** done first\n" );
	sleep( 20 );
	fprintf( stdout, "\n\n" );

	fprintf( stdout, "**main** dispatch 3\n" );
	pool.dispatch( threadFunc, (void *) 3 );
	fprintf( stdout, "**main** dispatch 6\n" );
	pool.dispatch( threadFunc, (void *) 6 );
	fprintf( stdout, "**main** dispatch 7\n");
	pool.dispatch( threadFunc, (void *) 7 );

	fprintf( stdout, "**main done second\n" );

	sleep(20);

	return 0;
}

