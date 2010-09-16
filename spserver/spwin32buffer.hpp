/*
 * This file is adapted from libevent.
 */

/*
 * Copyright (c) 2000-2007 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __spwin32buffer_hpp__
#define __spwin32buffer_hpp__

#include <sys/types.h>
#include <stdarg.h>

#define sp_evbuffer_new         spwin32buffer_new
#define sp_evbuffer_free        spwin32buffer_free
#define sp_evbuffer_add         spwin32buffer_add
#define sp_evbuffer_drain       spwin32buffer_drain
#define sp_evbuffer_expand      spwin32buffer_expand
#define sp_evbuffer_readline    spwin32buffer_readline
#define sp_evbuffer_remove      spwin32buffer_remove
#define sp_evbuffer_add_vprintf spwin32buffer_add_vprintf

typedef struct spwin32buffer sp_buffer_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u_char;

/* These functions deal with buffering input and output */

struct spwin32buffer {
	u_char *buffer;
	u_char *orig_buffer;

	size_t misalign;
	size_t totallen;
	size_t off;

	void (*cb)(struct spwin32buffer *, size_t, size_t, void *);
	void *cbarg;
};

#define EVBUFFER_LENGTH(x)	(x)->off
#define EVBUFFER_DATA(x)	(x)->buffer
#define EVBUFFER_INPUT(x)	(x)->input
#define EVBUFFER_OUTPUT(x)	(x)->output

/**
  Allocate storage for a new spwin32buffer.

  @return a pointer to a newly allocated spwin32buffer struct, or NULL if an error
          occurred
 */
struct spwin32buffer *spwin32buffer_new(void);


/**
  Deallocate storage for an spwin32buffer.

  @param pointer to the spwin32buffer to be freed
 */
void spwin32buffer_free(struct spwin32buffer *);


/**
  Expands the available space in an event buffer.

  Expands the available space in the event buffer to at least datlen

  @param buf the event buffer to be expanded
  @param datlen the new minimum length requirement
  @return 0 if successful, or -1 if an error occurred
*/
int spwin32buffer_expand(struct spwin32buffer *, size_t);


/**
  Append data to the end of an spwin32buffer.

  @param buf the event buffer to be appended to
  @param data pointer to the beginning of the data buffer
  @param datlen the number of bytes to be copied from the data buffer
 */
int spwin32buffer_add(struct spwin32buffer *, const void *, size_t);



/**
  Read data from an event buffer and drain the bytes read.

  @param buf the event buffer to be read from
  @param data the destination buffer to store the result
  @param datlen the maximum size of the destination buffer
  @return the number of bytes read
 */
int spwin32buffer_remove(struct spwin32buffer *, void *, size_t);


/**
 * Read a single line from an event buffer.
 *
 * Reads a line terminated by either '\r\n', '\n\r' or '\r' or '\n'.
 * The returned buffer needs to be freed by the caller.
 *
 * @param buffer the spwin32buffer to read from
 * @return pointer to a single line, or NULL if an error occurred
 */
char *spwin32buffer_readline(struct spwin32buffer *);


/**
  Move data from one spwin32buffer into another spwin32buffer.

  This is a destructive add.  The data from one buffer moves into
  the other buffer. The destination buffer is expanded as needed.

  @param outbuf the output buffer
  @param inbuf the input buffer
  @return 0 if successful, or -1 if an error occurred
 */
int spwin32buffer_add_buffer(struct spwin32buffer *, struct spwin32buffer *);


/**
  Append a formatted string to the end of an spwin32buffer.

  @param buf the spwin32buffer that will be appended to
  @param fmt a format string
  @param ... arguments that will be passed to printf(3)
  @return 0 if successful, or -1 if an error occurred
 */
int spwin32buffer_add_printf(struct spwin32buffer *, const char *fmt, ...)
#ifdef __GNUC__
  __attribute__((format(printf, 2, 3)))
#endif
;


/**
  Append a va_list formatted string to the end of an spwin32buffer.

  @param buf the spwin32buffer that will be appended to
  @param fmt a format string
  @param ap a varargs va_list argument array that will be passed to vprintf(3)
  @return 0 if successful, or -1 if an error occurred
 */
int spwin32buffer_add_vprintf(struct spwin32buffer *, const char *fmt, va_list ap);


/**
  Remove a specified number of bytes data from the beginning of an spwin32buffer.

  @param buf the spwin32buffer to be drained
  @param len the number of bytes to drain from the beginning of the buffer
  @return 0 if successful, or -1 if an error occurred
 */
void spwin32buffer_drain(struct spwin32buffer *, size_t);


/**
  Write the contents of an spwin32buffer to a file descriptor.

  The spwin32buffer will be drained after the bytes have been successfully written.

  @param buffer the spwin32buffer to be written and drained
  @param fd the file descriptor to be written to
  @return the number of bytes written, or -1 if an error occurred
  @see spwin32buffer_read()
 */
int spwin32buffer_write(struct spwin32buffer *, int);


/**
  Read from a file descriptor and store the result in an spwin32buffer.

  @param buf the spwin32buffer to store the result
  @param fd the file descriptor to read from
  @param howmuch the number of bytes to be read
  @return the number of bytes read, or -1 if an error occurred
  @see spwin32buffer_write()
 */
int spwin32buffer_read(struct spwin32buffer *, int, int);


/**
  Find a string within an spwin32buffer.

  @param buffer the spwin32buffer to be searched
  @param what the string to be searched for
  @param len the length of the search string
  @return a pointer to the beginning of the search string, or NULL if the search failed.
 */
u_char *spwin32buffer_find(struct spwin32buffer *, const u_char *, size_t);

/**
  Set a callback to invoke when the spwin32buffer is modified.

  @param buffer the spwin32buffer to be monitored
  @param cb the callback function to invoke when the spwin32buffer is modified
  @param cbarg an argument to be provided to the callback function
 */
void spwin32buffer_setcb(struct spwin32buffer *, void (*)(struct spwin32buffer *, size_t, size_t, void *), void *);

#ifdef __cplusplus
}
#endif

#endif

