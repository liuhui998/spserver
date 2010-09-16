/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */


#ifndef __sphandler_hpp__
#define __sphandler_hpp__

class SP_Buffer;
class SP_Request;
class SP_Response;
class SP_Message;

struct event;
struct timeval;

class SP_Handler {
public:
	virtual ~SP_Handler();

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response ) = 0;

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response ) = 0;

	virtual void error( SP_Response * response ) = 0;

	virtual void timeout( SP_Response * response ) = 0;

	virtual void close() = 0;
};

class SP_TimerHandler {
public:
	virtual ~SP_TimerHandler();

	// return -1 : terminate timer, 0 : continue
	virtual int handle( SP_Response * response, struct timeval * timeout ) = 0;
};

/**
 * @note Asynchronous Completion Token
 */
class SP_CompletionHandler {
public:
	virtual ~SP_CompletionHandler();

	virtual void completionMessage( SP_Message * msg ) = 0;
};

class SP_DefaultCompletionHandler : public SP_CompletionHandler {
public:
	SP_DefaultCompletionHandler();
	~SP_DefaultCompletionHandler();

	virtual void completionMessage( SP_Message * msg );
};

class SP_HandlerFactory {
public:
	virtual ~SP_HandlerFactory();

	virtual SP_Handler * create() const = 0;

	virtual SP_CompletionHandler * createCompletionHandler() const;
};

#endif

