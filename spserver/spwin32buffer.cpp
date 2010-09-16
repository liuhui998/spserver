/*
 * Copyright (c) 2002, 2003 Niels Provos <provos@citi.umich.edu>
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

#ifdef WIN32
#include <winsock2.h>
#pragma warning(disable: 4996)
#else
#include <unistd.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "spwin32buffer.hpp"

struct spwin32buffer *
spwin32buffer_new(void)
{
	struct spwin32buffer *buffer;
	
	buffer = (struct spwin32buffer*)calloc(1, sizeof(struct spwin32buffer));

	return (buffer);
}

void
spwin32buffer_free(struct spwin32buffer *buffer)
{
	if (buffer->orig_buffer != NULL)
		free(buffer->orig_buffer);
	free(buffer);
}

/* 
 * This is a destructive add.  The data from one buffer moves into
 * the other buffer.
 */

#define SWAP(x,y) do { \
	(x)->buffer = (y)->buffer; \
	(x)->orig_buffer = (y)->orig_buffer; \
	(x)->misalign = (y)->misalign; \
	(x)->totallen = (y)->totallen; \
	(x)->off = (y)->off; \
} while (0)

int
spwin32buffer_add_buffer(struct spwin32buffer *outbuf, struct spwin32buffer *inbuf)
{
	int res;

	/* Short cut for better performance */
	if (outbuf->off == 0) {
		struct spwin32buffer tmp;
		size_t oldoff = inbuf->off;

		/* Swap them directly */
		SWAP(&tmp, outbuf);
		SWAP(outbuf, inbuf);
		SWAP(inbuf, &tmp);

		/* 
		 * Optimization comes with a price; we need to notify the
		 * buffer if necessary of the changes. oldoff is the amount
		 * of data that we transfered from inbuf to outbuf
		 */
		if (inbuf->off != oldoff && inbuf->cb != NULL)
			(*inbuf->cb)(inbuf, oldoff, inbuf->off, inbuf->cbarg);
		if (oldoff && outbuf->cb != NULL)
			(*outbuf->cb)(outbuf, 0, oldoff, outbuf->cbarg);
		
		return (0);
	}

	res = spwin32buffer_add(outbuf, inbuf->buffer, inbuf->off);
	if (res == 0) {
		/* We drain the input buffer on success */
		spwin32buffer_drain(inbuf, inbuf->off);
	}

	return (res);
}

int
spwin32buffer_add_vprintf(struct spwin32buffer *buf, const char *fmt, va_list ap)
{
	char *buffer;
	size_t space;
	size_t oldoff = buf->off;
	int sz;
	va_list aq;

	/* make sure that at least some space is available */
	spwin32buffer_expand(buf, 64);
	for (;;) {
		size_t used = buf->misalign + buf->off;
		buffer = (char *)buf->buffer + buf->off;
		assert(buf->totallen >= used);
		space = buf->totallen - used;

#ifndef va_copy
#define	va_copy(dst, src)	memcpy(&(dst), &(src), sizeof(va_list))
#endif
		va_copy(aq, ap);

#ifdef WIN32
		sz = _vsnprintf(buffer, space - 1, fmt, aq);
		buffer[space - 1] = '\0';
#else
		sz = vsnprintf(buffer, space, fmt, aq);
#endif

		va_end(aq);

		if (sz < 0)
			return (-1);
		if (sz < (int)space) {
			buf->off += sz;
			if (buf->cb != NULL)
				(*buf->cb)(buf, oldoff, buf->off, buf->cbarg);
			return (sz);
		}
		if (spwin32buffer_expand(buf, sz + 1) == -1)
			return (-1);

	}
	/* NOTREACHED */
}

int
spwin32buffer_add_printf(struct spwin32buffer *buf, const char *fmt, ...)
{
	int res = -1;
	va_list ap;

	va_start(ap, fmt);
	res = spwin32buffer_add_vprintf(buf, fmt, ap);
	va_end(ap);

	return (res);
}

/* Reads data from an event buffer and drains the bytes read */

int
spwin32buffer_remove(struct spwin32buffer *buf, void *data, size_t datlen)
{
	size_t nread = datlen;
	if (nread >= buf->off)
		nread = buf->off;

	memcpy(data, buf->buffer, nread);
	spwin32buffer_drain(buf, nread);
	
	return (nread);
}

/*
 * Reads a line terminated by either '\r\n', '\n\r' or '\r' or '\n'.
 * The returned buffer needs to be freed by the called.
 */

char *
spwin32buffer_readline(struct spwin32buffer *buffer)
{
	u_char *data = EVBUFFER_DATA(buffer);
	size_t len = EVBUFFER_LENGTH(buffer);
	char *line;
	unsigned int i;

	for (i = 0; i < len; i++) {
		if (data[i] == '\r' || data[i] == '\n')
			break;
	}

	if (i == len)
		return (NULL);

	if ((line = (char*)malloc(i + 1)) == NULL) {
		//fprintf(stderr, "%s: out of memory\n", __func__);
		spwin32buffer_drain(buffer, i);
		return (NULL);
	}

	memcpy(line, data, i);
	line[i] = '\0';

	/*
	 * Some protocols terminate a line with '\r\n', so check for
	 * that, too.
	 */
	if ( i < len - 1 ) {
		char fch = data[i], sch = data[i+1];

		/* Drain one more character if needed */
		if ( (sch == '\r' || sch == '\n') && sch != fch )
			i += 1;
	}

	spwin32buffer_drain(buffer, i + 1);

	return (line);
}

/* Adds data to an event buffer */

static void
spwin32buffer_align(struct spwin32buffer *buf)
{
	memmove(buf->orig_buffer, buf->buffer, buf->off);
	buf->buffer = buf->orig_buffer;
	buf->misalign = 0;
}

/* Expands the available space in the event buffer to at least datlen */

int
spwin32buffer_expand(struct spwin32buffer *buf, size_t datlen)
{
	size_t need = buf->misalign + buf->off + datlen;

	/* If we can fit all the data, then we don't have to do anything */
	if (buf->totallen >= need)
		return (0);

	/*
	 * If the misalignment fulfills our data needs, we just force an
	 * alignment to happen.  Afterwards, we have enough space.
	 */
	if (buf->misalign >= datlen) {
		spwin32buffer_align(buf);
	} else {
		void *newbuf;
		size_t length = buf->totallen;

		if (length < 256)
			length = 256;
		while (length < need)
			length <<= 1;

		if (buf->orig_buffer != buf->buffer)
			spwin32buffer_align(buf);
		if ((newbuf = realloc(buf->buffer, length)) == NULL)
			return (-1);

		buf->orig_buffer = buf->buffer = (u_char*)newbuf;
		buf->totallen = length;
	}

	return (0);
}

int
spwin32buffer_add(struct spwin32buffer *buf, const void *data, size_t datlen)
{
	size_t need = buf->misalign + buf->off + datlen;
	size_t oldoff = buf->off;

	if (buf->totallen < need) {
		if (spwin32buffer_expand(buf, datlen) == -1)
			return (-1);
	}

	memcpy(buf->buffer + buf->off, data, datlen);
	buf->off += datlen;

	if (datlen && buf->cb != NULL)
		(*buf->cb)(buf, oldoff, buf->off, buf->cbarg);

	return (0);
}

void
spwin32buffer_drain(struct spwin32buffer *buf, size_t len)
{
	size_t oldoff = buf->off;

	if (len >= buf->off) {
		buf->off = 0;
		buf->buffer = buf->orig_buffer;
		buf->misalign = 0;
		goto done;
	}

	buf->buffer += len;
	buf->misalign += len;

	buf->off -= len;

 done:
	/* Tell someone about changes in this buffer */
	if (buf->off != oldoff && buf->cb != NULL)
		(*buf->cb)(buf, oldoff, buf->off, buf->cbarg);

}

/*
 * Reads data from a file descriptor into a buffer.
 */

#define EVBUFFER_MAX_READ	4096

int
spwin32buffer_read(struct spwin32buffer *buf, int fd, int howmuch)
{
	u_char *p;
	size_t oldoff = buf->off;
	int n = EVBUFFER_MAX_READ;

#if defined(FIONREAD)
#ifdef WIN32
	long lng = n;
	if (ioctlsocket(fd, FIONREAD, (unsigned long*)&lng) == -1 || (n=lng) == 0) {
#else
	if (ioctl(fd, FIONREAD, &n) == -1 || n == 0) {
#endif
		n = EVBUFFER_MAX_READ;
	} else if (n > EVBUFFER_MAX_READ && n > howmuch) {
		/*
		 * It's possible that a lot of data is available for
		 * reading.  We do not want to exhaust resources
		 * before the reader has a chance to do something
		 * about it.  If the reader does not tell us how much
		 * data we should read, we artifically limit it.
		 */
		if (n > (int)buf->totallen << 2)
			n = buf->totallen << 2;
		if (n < EVBUFFER_MAX_READ)
			n = EVBUFFER_MAX_READ;
	}
#endif	
	if (howmuch < 0 || howmuch > n)
		howmuch = n;

	/* If we don't have FIONREAD, we might waste some space here */
	if (spwin32buffer_expand(buf, howmuch) == -1)
		return (-1);

	/* We can append new data at this point */
	p = buf->buffer + buf->off;

#ifndef WIN32
	n = read(fd, p, howmuch);
#else
	n = recv(fd, (char*)p, howmuch, 0);
#endif
	if (n == -1)
		return (-1);
	if (n == 0)
		return (0);

	buf->off += n;

	/* Tell someone about changes in this buffer */
	if (buf->off != oldoff && buf->cb != NULL)
		(*buf->cb)(buf, oldoff, buf->off, buf->cbarg);

	return (n);
}

int
spwin32buffer_write(struct spwin32buffer *buffer, int fd)
{
	int n;

#ifndef WIN32
	n = write(fd, buffer->buffer, buffer->off);
#else
	n = send(fd, (char*)buffer->buffer, buffer->off, 0);
#endif
	if (n == -1)
		return (-1);
	if (n == 0)
		return (0);
	spwin32buffer_drain(buffer, n);

	return (n);
}

u_char *
spwin32buffer_find(struct spwin32buffer *buffer, const u_char *what, size_t len)
{
	u_char *search = buffer->buffer, *end = search + buffer->off;
	u_char *p;

	while (search < end &&
	    (p = (u_char*)memchr(search, *what, end - search)) != NULL) {
		if (p + len > end)
			break;
		if (memcmp(p, what, len) == 0)
			return (p);
		search = p + 1;
	}

	return (NULL);
}

void spwin32buffer_setcb(struct spwin32buffer *buffer,
    void (*cb)(struct spwin32buffer *, size_t, size_t, void *),
    void *cbarg)
{
	buffer->cb = cb;
	buffer->cbarg = cbarg;
}

