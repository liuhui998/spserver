/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <time.h>
#include <string.h>

#include "spthread.hpp"

#include "sphttp.hpp"
#include "sphttpmsg.hpp"
#include "spbuffer.hpp"
#include "sprequest.hpp"
#include "spresponse.hpp"
#include "spmsgblock.hpp"

SP_HttpHandler :: ~SP_HttpHandler()
{
}

void SP_HttpHandler :: error()
{
}

void SP_HttpHandler :: timeout()
{
}

//---------------------------------------------------------

SP_HttpHandlerFactory :: ~SP_HttpHandlerFactory()
{
}

//---------------------------------------------------------

class SP_HttpRequestDecoder : public SP_MsgDecoder {
public:
	SP_HttpRequestDecoder();

	virtual ~SP_HttpRequestDecoder();

	virtual int decode( SP_Buffer * inBuffer );

	SP_HttpRequest * getMsg();

private:
	SP_HttpMsgParser * mParser;
};

SP_HttpRequestDecoder :: SP_HttpRequestDecoder()
{
	mParser = new SP_HttpMsgParser();
}

SP_HttpRequestDecoder :: ~SP_HttpRequestDecoder()
{
	delete mParser;
}

int SP_HttpRequestDecoder :: decode( SP_Buffer * inBuffer )
{
	if( inBuffer->getSize() > 0 ) {
		int len = mParser->append( inBuffer->getBuffer(), inBuffer->getSize() );

		inBuffer->erase( len );

		return mParser->isCompleted() ? eOK : eMoreData;
	} else {
		return eMoreData;
	}
}

SP_HttpRequest * SP_HttpRequestDecoder :: getMsg()
{
	return mParser->getRequest();
}

//---------------------------------------------------------

class SP_HttpResponseMsgBlock : public SP_MsgBlock {
public:
	SP_HttpResponseMsgBlock( SP_HttpResponse * response );
	virtual ~SP_HttpResponseMsgBlock();

	virtual const void * getData() const;
	virtual size_t getSize() const;

private:
	SP_HttpResponse * mResponse;
};


SP_HttpResponseMsgBlock :: SP_HttpResponseMsgBlock( SP_HttpResponse * response )
{
	mResponse = response;
}

SP_HttpResponseMsgBlock :: ~SP_HttpResponseMsgBlock()
{
	if( NULL != mResponse ) delete mResponse;
	mResponse = NULL;
}

const void * SP_HttpResponseMsgBlock :: getData() const
{
	return mResponse->getContent();
}

size_t SP_HttpResponseMsgBlock :: getSize() const
{
	return mResponse->getContentLength();
}

//---------------------------------------------------------

class SP_HttpHandlerAdapter : public SP_Handler {
public:
	SP_HttpHandlerAdapter( SP_HttpHandler * handler );

	virtual ~SP_HttpHandlerAdapter();

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response );

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response );

	virtual void error( SP_Response * response );

	virtual void timeout( SP_Response * response );

	virtual void close();

private:
	SP_HttpHandler * mHandler;
};

SP_HttpHandlerAdapter :: SP_HttpHandlerAdapter( SP_HttpHandler * handler )
{
	mHandler = handler;
}

SP_HttpHandlerAdapter :: ~SP_HttpHandlerAdapter()
{
	delete mHandler;
}

int SP_HttpHandlerAdapter :: start( SP_Request * request, SP_Response * response )
{
	request->setMsgDecoder( new SP_HttpRequestDecoder() );

	return 0;
}

int SP_HttpHandlerAdapter :: handle( SP_Request * request, SP_Response * response )
{
	SP_HttpRequestDecoder * decoder = ( SP_HttpRequestDecoder * ) request->getMsgDecoder();
	SP_HttpRequest * httpRequest = ( SP_HttpRequest * ) decoder->getMsg();

	httpRequest->setClinetIP( request->getClientIP() );

	SP_HttpResponse * httpResponse = new SP_HttpResponse();
	httpResponse->setVersion( httpRequest->getVersion() );

	mHandler->handle( httpRequest, httpResponse );

	SP_Buffer * reply = response->getReply()->getMsg();

	char buffer[ 512 ] = { 0 };
	snprintf( buffer, sizeof( buffer ), "%s %i %s\r\n", httpResponse->getVersion(),
		httpResponse->getStatusCode(), httpResponse->getReasonPhrase() );
	reply->append( buffer );

	// check keep alive header
	if( httpRequest->isKeepAlive() ) {
		if( NULL == httpResponse->getHeaderValue( SP_HttpMessage::HEADER_CONNECTION ) ) {
			httpResponse->addHeader( SP_HttpMessage::HEADER_CONNECTION, "Keep-Alive" );
		}
	}

	if( 0 != strcasecmp( httpRequest->getMethod(), "head" ) ) {
		// check Content-Length header
		httpResponse->removeHeader( SP_HttpMessage::HEADER_CONTENT_LENGTH );
		if( httpResponse->getContentLength() >= 0 ) {
				snprintf( buffer, sizeof( buffer ), "%d", httpResponse->getContentLength() );
				httpResponse->addHeader( SP_HttpMessage::HEADER_CONTENT_LENGTH, buffer );
		}
	}

	// check date header
	httpResponse->removeHeader( SP_HttpMessage::HEADER_DATE );
	time_t tTime = time( NULL );
	struct tm tmTime;
	gmtime_r( &tTime, &tmTime );
	strftime( buffer, sizeof( buffer ), "%a, %d %b %Y %H:%M:%S %Z", &tmTime );
	httpResponse->addHeader( SP_HttpMessage::HEADER_DATE, buffer );

	// check Content-Type header
	if( NULL == httpResponse->getHeaderValue( SP_HttpMessage::HEADER_CONTENT_TYPE ) ) {
		httpResponse->addHeader( SP_HttpMessage::HEADER_CONTENT_TYPE,
			"text/html; charset=ISO-8859-1" );
	}

	// check Server header
	httpResponse->removeHeader( SP_HttpMessage::HEADER_SERVER );
	httpResponse->addHeader( SP_HttpMessage::HEADER_SERVER, "sphttp/spserver" );

	for( int i = 0; i < httpResponse->getHeaderCount(); i++ ) {
		snprintf( buffer, sizeof( buffer ), "%s: %s\r\n",
			httpResponse->getHeaderName( i ), httpResponse->getHeaderValue( i ) );
		reply->append( buffer );
	}

	reply->append( "\r\n" );	

	char keepAlive[ 32 ] = { 0 };
	if( NULL != httpResponse->getHeaderValue( SP_HttpMessage::HEADER_CONNECTION ) ) {
		strncpy( keepAlive, httpResponse->getHeaderValue(
				SP_HttpMessage::HEADER_CONNECTION ), sizeof( keepAlive ) - 1 );
	}

	if( NULL != httpResponse->getContent() ) {
		response->getReply()->getFollowBlockList()->append(
				new SP_HttpResponseMsgBlock( httpResponse ) );
	} else {
		delete httpResponse;
	}

	request->setMsgDecoder( new SP_HttpRequestDecoder() );

	return 0 == strcasecmp( keepAlive, "Keep-Alive" ) ? 0 : -1;
}

void SP_HttpHandlerAdapter :: error( SP_Response * response )
{
	mHandler->error();
}

void SP_HttpHandlerAdapter :: timeout( SP_Response * response )
{
	mHandler->timeout();
}

void SP_HttpHandlerAdapter :: close()
{
}

//---------------------------------------------------------

SP_HttpHandlerAdapterFactory :: SP_HttpHandlerAdapterFactory( SP_HttpHandlerFactory * factory )
{
	mFactory = factory;
}

SP_HttpHandlerAdapterFactory :: ~SP_HttpHandlerAdapterFactory()
{
	delete mFactory;
}

SP_Handler * SP_HttpHandlerAdapterFactory :: create() const
{
	return new SP_HttpHandlerAdapter( mFactory->create() );
}

