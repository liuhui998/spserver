/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>

#include "spmsgdecoder.hpp"
#include "spbuffer.hpp"

#include "spserver.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"
#include "spgnutls.hpp"
#include "sputils.hpp"

class SP_EchoHandler : public SP_Handler {
public:
	SP_EchoHandler(){}
	virtual ~SP_EchoHandler(){}

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response ) {
		request->setMsgDecoder( new SP_MultiLineMsgDecoder() );
		response->getReply()->getMsg()->append(
			"Welcome to line echo server, enter 'quit' to quit.\r\n" );

		return 0;
	}

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response ) {
		SP_MultiLineMsgDecoder * decoder = (SP_MultiLineMsgDecoder*)request->getMsgDecoder();
		SP_CircleQueue * queue = decoder->getQueue();

		int ret = 0;
		for( ; NULL != queue->top(); ) {
			char * line = (char*)queue->pop();

			if( 0 != strcasecmp( line, "quit" ) ) {
				response->getReply()->getMsg()->append( line );
				response->getReply()->getMsg()->append( "\r\n" );
			} else {
				response->getReply()->getMsg()->append( "Byebye\r\n" );
				ret = -1;
			}

			free( line );
		}

		return ret;
	}

	virtual void error( SP_Response * response ) {}

	virtual void timeout( SP_Response * response ) {}

	virtual void close() {}
};

class SP_EchoHandlerFactory : public SP_HandlerFactory {
public:
	SP_EchoHandlerFactory() {}
	virtual ~SP_EchoHandlerFactory() {}

	virtual SP_Handler * create() const {
		return new SP_EchoHandler();
	}
};

//---------------------------------------------------------

int main( int argc, char * argv[] )
{
#ifdef LOG_PERROR
	openlog( "testechos", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );
#else
	openlog( "testechos", LOG_CONS | LOG_PID, LOG_USER );
#endif

	int port = 1995;

	SP_Server server( "", port, new SP_EchoHandlerFactory() );

	SP_GnutlsChannelFactory * factory = new SP_GnutlsChannelFactory();
	factory->init( "demo.crt", "demo.key" );
	server.setIOChannelFactory( factory );

	server.runForever();

	return 0;
}

