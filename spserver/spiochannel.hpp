/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spiochannel_hpp__
#define __spiochannel_hpp__

class SP_Session;
class SP_Buffer;

#ifdef WIN32
typedef struct spwin32buffer sp_evbuffer_t;
#else
typedef struct evbuffer sp_evbuffer_t;
#endif

struct iovec;

class SP_IOChannel {
public:
	virtual ~SP_IOChannel();

	// call by an independence thread, can block
	// return -1 : terminate session, 0 : continue
	virtual int init( int fd ) = 0;

	// run in event-loop thread, cannot block
	// return the number of bytes received, or -1 if an error occurred.
	virtual int receive( SP_Session * session ) = 0;

	// run in event-loop thread, cannot block
	// return the number of bytes sent, or -1 if an error occurred.
	virtual int transmit( SP_Session * session );

protected:
	static sp_evbuffer_t * getEvBuffer( SP_Buffer * buffer );

	// returns the number of bytes written, or -1 if an error occurred.
	virtual int write_vec( struct iovec * iovArray, int iovSize ) = 0;
};

class SP_IOChannelFactory {
public:
	virtual ~SP_IOChannelFactory();

	virtual SP_IOChannel * create() const = 0;
};

class SP_DefaultIOChannelFactory : public SP_IOChannelFactory {
public:
	SP_DefaultIOChannelFactory();
	virtual ~SP_DefaultIOChannelFactory();

	virtual SP_IOChannel * create() const;
};

class SP_DefaultIOChannel : public SP_IOChannel {
public:
	SP_DefaultIOChannel();
	~SP_DefaultIOChannel();

	virtual int init( int fd );
	virtual int receive( SP_Session * session );

protected:
	virtual int write_vec( struct iovec * iovArray, int iovSize );
	int mFd;
};

#endif

