/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "spporting.hpp"

#include "sphttpmsg.hpp"
#include "sputils.hpp"

static char * sp_strsep(char **s, const char *del)
{
	char *d, *tok;

	if (!s || !*s) return NULL;
	tok = *s;
	d = strstr(tok, del);

	if (d) {
		*s = d + strlen(del);
		*d = '\0';
	} else {
		*s = NULL;
	}

	return tok;
}

SP_HttpMsgParser :: SP_HttpMsgParser()
{
	mMessage = NULL;
	mStatus = eStartLine;
	mIgnoreContent = 0;
}

SP_HttpMsgParser :: ~SP_HttpMsgParser()
{
	if( NULL != mMessage ) delete mMessage;
}

void SP_HttpMsgParser :: setIgnoreContent( int ignoreContent )
{
	mIgnoreContent = ignoreContent;
}

int SP_HttpMsgParser :: isIgnoreContent() const
{
	return 0 != mIgnoreContent;
}

int SP_HttpMsgParser :: parseStartLine( SP_HttpMessage ** message,
		const void * buffer, int len )
{
	int lineLen = 0;

	char * pos = (char*)memchr( buffer, '\n', len );
	if( NULL != pos ) {
		lineLen = pos - (char*)buffer + 1;

		char * line = (char*)malloc( lineLen + 1 );
		memcpy( line, buffer, lineLen );
		line[ lineLen ] = '\0';

		pos = line;

		char * first, * second;
		first = sp_strsep( &pos, " " );
		second = sp_strsep( &pos, " " );

		if( 0 == strncasecmp( line, "HTTP", 4 ) ) {
			SP_HttpResponse * response = new SP_HttpResponse();
			if( NULL != first ) response->setVersion( first );
			if( NULL != second ) response->setStatusCode( atoi( second ) );
			if( NULL != pos ) response->setReasonPhrase( strtok( pos, "\r\n" ) );

			*message = response;
		} else {
			SP_HttpRequest * request = new SP_HttpRequest();
			if( NULL != first ) request->setMethod( first );
			if( NULL != second ) request->setURL( second );
			if( NULL != second ) request->setURI( sp_strsep( &second, "?" ) );
			if( NULL != pos ) request->setVersion( strtok( pos, "\r\n" ) );

			char * params = second;
			for( ; NULL != params && '\0' != *params; ) {
				char * value = sp_strsep( &params, "&" );
				char * name = sp_strsep( &value, "=" );
				request->addParam( name, NULL == value ? "" : value );
			}

			*message = request;
		}

		free( line );
	}

	return lineLen;
}

int SP_HttpMsgParser :: parseHeader( SP_HttpMessage * message,
		const void * buffer, int len )
{
	int lineLen = 0;

	char * pos = (char*)memchr( buffer, '\n', len );
	if( NULL != pos ) {
		lineLen = pos - (char*)buffer + 1;

		char * line = (char*)malloc( lineLen + 1 );
		memcpy( line, buffer, lineLen );
		line[ lineLen ] = '\0';

		pos = line;

		char * name = sp_strsep( &pos, ":" );

		if( NULL != pos ) {
			pos = strtok( pos, "\r\n" );
			pos += strspn( pos, " " );
			message->addHeader( name, pos );
		}

		free( line );
	}

	return lineLen;
}

int SP_HttpMsgParser :: getLine( const void * buffer, int len,
		char * line, int size )
{
	int lineLen = 0;

	char * pos = (char*)memchr( buffer, '\n', len );
	if( NULL != pos ) {
		lineLen = pos - (char*)buffer + 1;

		int realLen = size - 1;
		realLen = realLen > lineLen ? lineLen : realLen;

		memcpy( line, buffer, realLen );
		line[ realLen ] = '\0';
		strtok( line, "\r\n" );
	}

	return lineLen;
}

int SP_HttpMsgParser :: parseChunked( SP_HttpMessage * message,
		const void * buffer, int len, int * status )
{
	int parsedLen = 0, hasChunk = 1;

	for( ; 0 != hasChunk && eCompleted != *status; ) {

		hasChunk = 0;

		char chunkSize[ 32 ] = { 0 };

		int lineLen = getLine( ((char*)buffer) + parsedLen, len - parsedLen,
				chunkSize, sizeof( chunkSize ) );

		int contentLen = strtol( chunkSize, NULL, 16 );

		if( contentLen > 0 && ( len - parsedLen ) > ( contentLen + lineLen ) ) {
			int emptyLen = getLine( ((char*)buffer) + parsedLen + lineLen + contentLen,
				len - parsedLen - lineLen - contentLen,
				chunkSize, sizeof( chunkSize ) );

			if( emptyLen > 0 ) {
				parsedLen += lineLen;
				message->appendContent( ((char*)buffer) + parsedLen, contentLen );
				parsedLen += contentLen + emptyLen;
				hasChunk = 1;
			}
		}

		if( 0 == contentLen && lineLen > 0 ) {
			parsedLen += lineLen;
			*status = eCompleted;
		}
	}

	return parsedLen;
}

int SP_HttpMsgParser :: parseContent( SP_HttpMessage * message,
		const void * buffer, int len, int * status )
{
	int parsedLen = 0;

	const char * value = message->getHeaderValue( SP_HttpMessage::HEADER_CONTENT_LENGTH );
	int contentLen = atoi( NULL == value ? "0" : value );

	if( contentLen > 0 && len >= contentLen ) {
		message->appendContent( ((char*)buffer), contentLen );
		parsedLen = contentLen;
	}

	if( contentLen == message->getContentLength() ) *status = eCompleted;

	return parsedLen;
}

int SP_HttpMsgParser :: append( const void * buffer, int len )
{
	int parsedLen = 0;

	if( eCompleted == mStatus ) return parsedLen;

	// parse start-line
	if( NULL == mMessage ) {
		parsedLen = parseStartLine( &mMessage, buffer, len );
		if( parsedLen > 0 ) mStatus = eHeader;
	}

	if( NULL != mMessage ) {
		// parse header
		for( int headerLen = 1; eHeader == mStatus
				&& headerLen > 0 && parsedLen < len; parsedLen += headerLen ) {
			headerLen = parseHeader( mMessage, ((char*)buffer) + parsedLen, len - parsedLen );

			char ch = * ( ((char*)buffer) + parsedLen );
			if( '\r' == ch || '\n' == ch ) mStatus = eContent;
		}

		if( SP_HttpMessage::eResponse == mMessage->getType()
			&& eContent == mStatus && mIgnoreContent ) mStatus = eCompleted;

		// parse content
		if( eContent == mStatus ) {
			const char * encoding = mMessage->getHeaderValue( SP_HttpMessage::HEADER_TRANSFER_ENCODING );
			if( NULL != encoding && 0 == strcasecmp( encoding, "chunked" ) ) {
				parsedLen += parseChunked( mMessage, ((char*)buffer) + parsedLen,
					len - parsedLen, &mStatus );
			} else {
				parsedLen += parseContent( mMessage, ((char*)buffer) + parsedLen,
					len - parsedLen, &mStatus );
			}
		}

		if( eCompleted == mStatus ) postProcess( mMessage );
	}

	return parsedLen;
}

void SP_HttpMsgParser :: postProcess( SP_HttpMessage * message )
{
	if( SP_HttpMessage::eRequest == message->getType() ) {
		SP_HttpRequest * request = (SP_HttpRequest*)message;
		const char * contentType = request->getHeaderValue(
			SP_HttpMessage::HEADER_CONTENT_TYPE );
		if( request->getContentLength() > 0 && NULL != contentType
			&& 0 == strcasecmp( contentType, "application/x-www-form-urlencoded" ) ) {

			char * content = (char*)malloc( request->getContentLength() + 1 );
			memcpy( content, request->getContent(), request->getContentLength() );
			content[ request->getContentLength() ] = '\0';

			char * params = content;
			for( ; NULL != params && '\0' != *params; ) {
				char * value = sp_strsep( &params, "&" );
				char * name = sp_strsep( &value, "=" );
				request->addParam( name, NULL == value ? "" : value );
			}

			free( content );
		}
	}
}

int SP_HttpMsgParser :: isCompleted() const
{
	return eCompleted == mStatus;
}

SP_HttpRequest * SP_HttpMsgParser :: getRequest() const
{
	if( NULL != mMessage && SP_HttpMessage::eRequest == mMessage->getType() ) {
		return (SP_HttpRequest*)mMessage;
	}

	return NULL;
}

SP_HttpResponse * SP_HttpMsgParser :: getResponse() const
{
	if( NULL != mMessage && SP_HttpMessage::eResponse== mMessage->getType() ) {
		return (SP_HttpResponse*)mMessage;
	}

	return NULL;
}

//---------------------------------------------------------

const char * SP_HttpMessage :: HEADER_CONTENT_LENGTH = "Content-Length";
const char * SP_HttpMessage :: HEADER_CONTENT_TYPE = "Content-Type";
const char * SP_HttpMessage :: HEADER_CONNECTION = "Connection";
const char * SP_HttpMessage :: HEADER_PROXY_CONNECTION = "Proxy-Connection";
const char * SP_HttpMessage :: HEADER_TRANSFER_ENCODING = "Transfer-Encoding";
const char * SP_HttpMessage :: HEADER_DATE = "Date";
const char * SP_HttpMessage :: HEADER_SERVER = "Server";

SP_HttpMessage :: SP_HttpMessage( int type )
	: mType( type )
{
	mContent = NULL;
	mContentLength = 0;
	mMaxLength = 0;

	mHeaderNameList = new SP_ArrayList();
	mHeaderValueList = new SP_ArrayList();

	snprintf( mVersion, sizeof( mVersion ), "%s", "HTTP/1.0" );
}

SP_HttpMessage :: ~SP_HttpMessage()
{
	for( int i = mHeaderNameList->getCount() - 1; i >= 0; i-- ) {
		free( mHeaderNameList->takeItem( i ) );
		free( mHeaderValueList->takeItem( i ) );
	}

	delete mHeaderNameList;
	delete mHeaderValueList;

	if( NULL != mContent ) free( mContent );
}

int SP_HttpMessage :: getType() const
{
	return mType;
}

void SP_HttpMessage :: setVersion( const char * version )
{
	snprintf( mVersion, sizeof( mVersion ), "%s", version );
}

const char * SP_HttpMessage :: getVersion() const
{
	return mVersion;
}

void SP_HttpMessage :: appendContent( const void * content, int length, int maxLength )
{
	if( length <= 0 ) length = strlen( (char*)content );

	int realLength = mContentLength + length;
	realLength = realLength > maxLength ? realLength : maxLength;

	if( realLength > mMaxLength ) {
		if( NULL == mContent ) {
			mContent = malloc( realLength + 1 );
		} else {
			mContent = realloc( mContent, realLength + 1 );
		}
		mMaxLength = realLength;
	}

	memcpy( ((char*)mContent) + mContentLength, content, length );
	mContentLength = mContentLength + length;

	((char*)mContent)[ mContentLength ] = '\0';
}

void SP_HttpMessage :: setContent( const void * content, int length )
{
	mContentLength = 0;
	appendContent( content, length );
}

void SP_HttpMessage :: directSetContent( void * content, int length )
{
	if( NULL != mContent ) free( mContent );

	length = length > 0 ? length : strlen( (char*)content );

	mContentLength = mMaxLength = length;
	mContent = content;
}

const void * SP_HttpMessage :: getContent() const
{
	return mContent;
}

int SP_HttpMessage :: getContentLength() const
{
	return mContentLength;
}

void SP_HttpMessage :: addHeader( const char * name, const char * value )
{
	mHeaderNameList->append( strdup( name ) );
	mHeaderValueList->append( strdup( value ) );
}

int SP_HttpMessage :: removeHeader( const char * name )
{
	int ret = 0;

	for( int i = 0; i < mHeaderNameList->getCount() && 0 == ret; i++ ) {
		if( 0 == strcasecmp( name, (char*)mHeaderNameList->getItem( i ) ) ) {
			free( mHeaderNameList->takeItem( i ) );	
			free( mHeaderValueList->takeItem( i ) );
			ret = 1;
		}
	}

	return ret;
}

int SP_HttpMessage :: removeHeader( int index )
{
	int ret = 0;

	if( index >= 0 && index < mHeaderNameList->getCount() ) {
		ret = 1;

		free( mHeaderNameList->takeItem( index ) );	
		free( mHeaderValueList->takeItem( index ) );
	}

	return ret;
}

int SP_HttpMessage :: getHeaderCount() const
{
	return mHeaderNameList->getCount();
}

const char * SP_HttpMessage :: getHeaderName( int index ) const
{
	return (char*)mHeaderNameList->getItem( index );
}

const char * SP_HttpMessage :: getHeaderValue( int index ) const
{
	return (char*)mHeaderValueList->getItem( index );
}

const char * SP_HttpMessage :: getHeaderValue( const char * name ) const
{
	const char * value = NULL;

	for( int i = 0; i < mHeaderNameList->getCount() && NULL == value; i++ ) {
		if( 0 == strcasecmp( name, (char*)mHeaderNameList->getItem( i ) ) ) {
			value = (char*)mHeaderValueList->getItem( i );
		}
	}

	return value;
}

int SP_HttpMessage :: isKeepAlive() const
{
	const char * proxy = getHeaderValue( HEADER_PROXY_CONNECTION );
	const char * local = getHeaderValue( HEADER_CONNECTION );

	if( ( NULL != proxy && 0 == strcasecmp( proxy, "Keep-Alive" ) )
			|| ( NULL != local && 0 == strcasecmp( local, "Keep-Alive" ) ) ) {
		return 1;
	}

	return 0;
}

//---------------------------------------------------------

SP_HttpRequest :: SP_HttpRequest()
	: SP_HttpMessage( eRequest )
{
	memset( mMethod, 0, sizeof( mMethod ) );
	memset( mClientIP, 0, sizeof( mClientIP ) );
	mURI = mURL = NULL;

	mParamNameList = new SP_ArrayList();
	mParamValueList = new SP_ArrayList();
}

SP_HttpRequest :: ~SP_HttpRequest()
{
	if( NULL != mURI ) free( mURI );
	if( NULL != mURL ) free( mURL );

	for( int i = mParamNameList->getCount() - 1; i >= 0; i-- ) {
		free( mParamNameList->takeItem( i ) );
		free( mParamValueList->takeItem( i ) );
	}
	delete mParamNameList;
	delete mParamValueList;
}

void SP_HttpRequest :: setMethod( const char * method )
{
	snprintf( mMethod, sizeof( mMethod ), "%s", method );
}

const char * SP_HttpRequest :: getMethod() const
{
	return mMethod;
}

void SP_HttpRequest :: setURI( const char * uri )
{
	char * temp = mURI;

	mURI = strdup( uri );

	if( NULL != temp ) free( temp );
}

const char * SP_HttpRequest :: getURI() const
{
	return mURI;
}

void SP_HttpRequest :: setURL( const char * url )
{
	char * temp = mURL;

	mURL = strdup( url );

	if( NULL != temp ) free( temp );
}

const char * SP_HttpRequest :: getURL() const
{
	return mURL;
}

void SP_HttpRequest :: setClinetIP( const char * clientIP )
{
	snprintf( mClientIP, sizeof( mClientIP ), "%s", clientIP );
}

const char * SP_HttpRequest :: getClientIP() const
{
	return mClientIP;
}

void SP_HttpRequest :: addParam( const char * name, const char * value )
{
	mParamNameList->append( strdup( name ) );
	mParamValueList->append( strdup( value ) );
}

int SP_HttpRequest :: removeParam( const char * name )
{
	int ret = 0;

	for( int i = 0; i < mParamNameList->getCount() && 0 == ret; i++ ) {
		if( 0 == strcasecmp( name, (char*)mParamNameList->getItem( i ) ) ) {
			free( mParamNameList->takeItem( i ) );	
			free( mParamValueList->takeItem( i ) );
			ret = 1;
		}
	}

	return ret;
}

int SP_HttpRequest :: getParamCount() const
{
	return mParamNameList->getCount();
}

const char * SP_HttpRequest :: getParamName( int index ) const
{
	return (char*)mParamNameList->getItem( index );
}

const char * SP_HttpRequest :: getParamValue( int index ) const
{
	return (char*)mParamValueList->getItem( index );
}

const char * SP_HttpRequest :: getParamValue( const char * name ) const
{
	const char * value = NULL;

	for( int i = 0; i < mParamNameList->getCount() && NULL == value; i++ ) {
		if( 0 == strcasecmp( name, (char*)mParamNameList->getItem( i ) ) ) {
			value = (char*)mParamValueList->getItem( i );
		}
	}

	return value;
}

//---------------------------------------------------------

SP_HttpResponse :: SP_HttpResponse()
	: SP_HttpMessage( eResponse )
{
	mStatusCode = 200;
	snprintf( mReasonPhrase, sizeof( mReasonPhrase ), "%s", "OK" );
}

SP_HttpResponse :: ~SP_HttpResponse()
{
}

void SP_HttpResponse :: setStatusCode( int statusCode )
{
	mStatusCode = statusCode;
}

int SP_HttpResponse :: getStatusCode() const
{
	return mStatusCode;
}

void SP_HttpResponse :: setReasonPhrase( const char * reasonPhrase )
{
	snprintf( mReasonPhrase, sizeof( mReasonPhrase ), "%s", reasonPhrase );
}

const char * SP_HttpResponse :: getReasonPhrase() const
{
	return mReasonPhrase;
}

//---------------------------------------------------------

