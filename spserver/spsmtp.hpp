/*
 * Copyright 2009 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spsmtp_hpp__
#define __spsmtp_hpp__

#include "sphandler.hpp"

class SP_Buffer;
class SP_ArrayList;

class SP_SmtpHandler {
public:
	virtual ~SP_SmtpHandler();

	virtual void error();

	virtual void timeout();

	enum {
		eAccept = 0,  // command accepted
		eReject = -1, // command rejected
		eClose  = -2  // force to close the connection
	};

	virtual int welcome( const char * clientIP, const char * serverIP, SP_Buffer * reply );

	virtual int help( const char * args, SP_Buffer * reply );

	virtual int helo( const char * args, SP_Buffer * reply );

	virtual int ehlo( const char * args, SP_Buffer * reply );

	/**
	 * Called after the AUTH LOGIN during a SMTP exchange.
	 *
	 * @param user is the encoded username
	 * @param pass is the encoded password
	 */
	virtual int auth( const char * user, const char * pass, SP_Buffer * reply );

	virtual int noop( const char * args, SP_Buffer * reply );

	/**
	 * Called first, after the MAIL FROM during a SMTP exchange.
	 *
	 * @param args is the args of the MAIL FROM
	 */
	virtual int from( const char * args, SP_Buffer * reply ) = 0;

	/**
	 * Called once for every RCPT TO during a SMTP exchange.
	 * This will occur after a from() call.
	 *
	 * @param args is the args of the RCPT TO
	 */
	virtual int rcpt( const char * args, SP_Buffer * reply ) = 0;

	/**
	 * Called when the DATA part of the SMTP exchange begins.  Will
	 * only be called if at least one recipient was accepted.
	 *
	 * @param data will be the smtp data stream, stripped of any extra '.' chars
	 */
	virtual int data( const char * data, SP_Buffer * reply ) = 0;

	/**
	 * This method is called whenever a RSET command is sent. It should
	 * be used to clean up any pending deliveries.
	 */
	virtual int rset( SP_Buffer * reply ) = 0;
};

class SP_SmtpHandlerList {
public:
	SP_SmtpHandlerList();
	~SP_SmtpHandlerList();

	int getCount();
	void append( SP_SmtpHandler * handler );
	SP_SmtpHandler * getItem( int index );

private:
	SP_ArrayList * mList;
};

class SP_SmtpHandlerFactory {
public:
	virtual ~SP_SmtpHandlerFactory();

	virtual SP_SmtpHandler * create() const = 0;
};

class SP_SmtpHandlerAdapterFactory : public SP_HandlerFactory {
public:
	SP_SmtpHandlerAdapterFactory( SP_SmtpHandlerFactory * factory );

	virtual ~SP_SmtpHandlerAdapterFactory();

	virtual SP_Handler * create() const;

private:
	SP_SmtpHandlerFactory * mFactory;
};

#endif

