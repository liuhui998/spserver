/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __sphttp_hpp__
#define __sphttp_hpp__

#include "sphandler.hpp"
#include "spmsgdecoder.hpp"

class SP_HttpRequest;
class SP_HttpResponse;
class SP_HttpMsgParser;

class SP_HttpHandler {
public:
	virtual ~SP_HttpHandler();

	virtual void handle( SP_HttpRequest * request, SP_HttpResponse * response ) = 0;

	virtual void error();

	virtual void timeout();
};

class SP_HttpHandlerFactory {
public:
	virtual ~SP_HttpHandlerFactory();

	virtual SP_HttpHandler * create() const = 0;
};

class SP_HttpHandlerAdapterFactory : public SP_HandlerFactory {
public:
	SP_HttpHandlerAdapterFactory( SP_HttpHandlerFactory * factory );

	virtual ~SP_HttpHandlerAdapterFactory();

	virtual SP_Handler * create() const;

private:
	SP_HttpHandlerFactory * mFactory;
};

#endif

