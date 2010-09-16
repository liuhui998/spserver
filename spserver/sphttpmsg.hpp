/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __sphttpmsg_hpp__
#define __sphttpmsg_hpp__

class SP_ArrayList;
class SP_HttpRequest;
class SP_HttpResponse;
class SP_HttpMessage;

class SP_HttpMsgParser {
public:
	SP_HttpMsgParser();
	~SP_HttpMsgParser();

	void setIgnoreContent( int ignoreContent );
	int isIgnoreContent() const;

	int append( const void * buffer, int len );

	// 0 : incomplete, 1 : complete
	int isCompleted() const;

	SP_HttpRequest * getRequest() const;

	SP_HttpResponse * getResponse() const;

private:
	static int parseStartLine( SP_HttpMessage ** message,
		const void * buffer, int len );
	static int parseHeader( SP_HttpMessage * message,
		const void * buffer, int len );
	static int parseChunked( SP_HttpMessage * message,
		const void * buffer, int len, int * status );
	static int parseContent( SP_HttpMessage * message,
		const void * buffer, int len, int * status );
	static void postProcess( SP_HttpMessage * message );

	static int getLine( const void * buffer, int len, char * line, int size );

	SP_HttpMessage * mMessage;

	enum { eStartLine, eHeader, eContent, eCompleted };
	int mStatus;

	int mIgnoreContent;
};

class SP_HttpMessage {
public:
	static const char * HEADER_CONTENT_LENGTH;
	static const char * HEADER_CONTENT_TYPE;
	static const char * HEADER_CONNECTION;
	static const char * HEADER_PROXY_CONNECTION;
	static const char * HEADER_TRANSFER_ENCODING;
	static const char * HEADER_DATE;
	static const char * HEADER_SERVER;

public:
	SP_HttpMessage( int type );
	virtual ~SP_HttpMessage();

	enum { eRequest, eResponse };
	int getType() const;

	void setVersion( const char * version );
	const char * getVersion() const;

	void appendContent( const void * content, int length = 0, int maxLength = 0 );
	void setContent( const void * content, int length = 0 );
	void directSetContent( void * content, int length = 0 );
	const void * getContent() const;
	int getContentLength() const;

	void addHeader( const char * name, const char * value );
	int removeHeader( const char * name );
	int removeHeader( int index );
	int getHeaderCount() const;
	const char * getHeaderName( int index ) const;
	const char * getHeaderValue( int index ) const;
	const char * getHeaderValue( const char * name ) const;

	int isKeepAlive() const;

protected:
	const int mType;

	char mVersion[ 16 ];
	void * mContent;
	int mMaxLength, mContentLength;

	SP_ArrayList * mHeaderNameList, * mHeaderValueList;
};

class SP_HttpRequest : public SP_HttpMessage {
public:
	SP_HttpRequest();
	virtual ~SP_HttpRequest();

	void setMethod( const char * method );
	const char * getMethod() const;

	void setURI( const char * uri );
	const char * getURI() const;

	void setURL( const char * url );
	const char * getURL() const;

	void setClinetIP( const char * clientIP );
	const char * getClientIP() const;

	void addParam( const char * name, const char * value );
	int removeParam( const char * name );
	int getParamCount() const;
	const char * getParamName( int index ) const;
	const char * getParamValue( int index ) const;
	const char * getParamValue( const char * name ) const;

private:
	char mMethod[ 16 ], mClientIP[ 16 ];
	char * mURI, * mURL;

	SP_ArrayList * mParamNameList, * mParamValueList;
};

class SP_HttpResponse : public SP_HttpMessage {
public:
	SP_HttpResponse();
	virtual ~SP_HttpResponse();

	void setStatusCode( int statusCode );
	int getStatusCode() const;

	void setReasonPhrase( const char * reasonPhrase );
	const char * getReasonPhrase() const;

private:
	int mStatusCode;
	char mReasonPhrase[ 128 ];
};

#endif

