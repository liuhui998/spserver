/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spgetopt_h__
#define __spgetopt_h__

#ifdef WIN32

#ifdef __cplusplus
extern "C" {
#endif
	
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char* const *argv, const char *optstr);

#ifdef __cplusplus
}
#endif

#endif

#endif
