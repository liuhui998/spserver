/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "spporting.hpp"

#include "speventcb.hpp"
#include "spexecutor.hpp"
#include "spsession.hpp"
#include "spresponse.hpp"
#include "sphandler.hpp"
#include "spbuffer.hpp"
#include "spmsgdecoder.hpp"
#include "sputils.hpp"
#include "sprequest.hpp"
#include "spmsgblock.hpp"
#include "spiochannel.hpp"
#include "spioutils.hpp"

#include "event_msgqueue.h"
#include "event.h"

SP_EventArg :: SP_EventArg( int timeout )
{
	mEventBase = (struct event_base*)event_init();

	mResponseQueue = msgqueue_new( mEventBase, 0,
			SP_EventCallback::onResponse, this );

	mInputResultQueue = new SP_BlockingQueue();

	mOutputResultQueue = new SP_BlockingQueue();

	mSessionManager = new SP_SessionManager();

	mTimeout = timeout;
}

SP_EventArg :: ~SP_EventArg()
{
	delete mInputResultQueue;
	delete mOutputResultQueue;

	delete mSessionManager;

	//msgqueue_destroy( (struct event_msgqueue*)mResponseQueue );
	//event_base_free( mEventBase );
}

struct event_base * SP_EventArg :: getEventBase() const
{
	return mEventBase;
}

void * SP_EventArg :: getResponseQueue() const
{
	return mResponseQueue;
}

SP_BlockingQueue * SP_EventArg :: getInputResultQueue() const
{
	return mInputResultQueue;
}

SP_BlockingQueue * SP_EventArg :: getOutputResultQueue() const
{
	return mOutputResultQueue;
}

SP_SessionManager * SP_EventArg :: getSessionManager() const
{
	return mSessionManager;
}

void SP_EventArg :: setTimeout( int timeout )
{
	mTimeout = timeout;
}

int SP_EventArg :: getTimeout() const
{
	return mTimeout;
}

//-------------------------------------------------------------------

void SP_EventCallback :: onAccept( int fd, short events, void * arg )
{
	int clientFD;
	struct sockaddr_in addr;
	socklen_t addrLen = sizeof( addr );

	SP_AcceptArg_t * acceptArg = (SP_AcceptArg_t*)arg;
	SP_EventArg * eventArg = acceptArg->mEventArg;

	clientFD = accept( fd, (struct sockaddr *)&addr, &addrLen );
	if( -1 == clientFD ) {
		sp_syslog( LOG_WARNING, "accept failed" );
		return;
	}

	if( SP_IOUtils::setNonblock( clientFD ) < 0 ) {
		sp_syslog( LOG_WARNING, "failed to set client socket non-blocking" );
	}

	SP_Sid_t sid;
	sid.mKey = eventArg->getSessionManager()->allocKey( &sid.mSeq );
	assert( sid.mKey > 0 );

	SP_Session * session = new SP_Session( sid );

	char strip[ 32 ] = { 0 };
	SP_IOUtils::inetNtoa( &( addr.sin_addr ), strip, sizeof( strip ) );
	session->getRequest()->setClientIP( strip );
	session->getRequest()->setClientPort( ntohs( addr.sin_port ) );

	if( 0 == getsockname( clientFD, (struct sockaddr*)&addr, &addrLen ) ) {
		SP_IOUtils::inetNtoa( &( addr.sin_addr ), strip, sizeof( strip ) );
		session->getRequest()->setServerIP( strip );
	}

	if( NULL != session ) {
		eventArg->getSessionManager()->put( sid.mKey, sid.mSeq, session );

		session->setHandler( acceptArg->mHandlerFactory->create() );
		session->setIOChannel( acceptArg->mIOChannelFactory->create() );
		session->setArg( eventArg );

		event_set( session->getReadEvent(), clientFD, EV_READ, onRead, session );
		event_set( session->getWriteEvent(), clientFD, EV_WRITE, onWrite, session );

		if( eventArg->getSessionManager()->getCount() > acceptArg->mMaxConnections
				|| eventArg->getInputResultQueue()->getLength() >= acceptArg->mReqQueueSize ) {
			sp_syslog( LOG_WARNING, "System busy, session.count %d [%d], queue.length %d [%d]",
				eventArg->getSessionManager()->getCount(), acceptArg->mMaxConnections,
				eventArg->getInputResultQueue()->getLength(), acceptArg->mReqQueueSize );

			SP_Message * msg = new SP_Message();
			msg->getMsg()->append( acceptArg->mRefusedMsg );
			msg->getMsg()->append( "\r\n" );
			session->getOutList()->append( msg );
			session->setStatus( SP_Session::eExit );

			addEvent( session, EV_WRITE, clientFD );
		} else {
			SP_EventHelper::doStart( session );
		}
	} else {
		eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );
		sp_close( clientFD );
		sp_syslog( LOG_WARNING, "Out of memory, cannot allocate session object!" );
	}
}

void SP_EventCallback :: onRead( int fd, short events, void * arg )
{
	SP_Session * session = (SP_Session*)arg;

	session->setReading( 0 );

	SP_Sid_t sid = session->getSid();

	if( EV_READ & events ) {
		int len = session->getIOChannel()->receive( session );

		if( len > 0 ) {
			session->addRead( len );
			if( 0 == session->getRunning() ) {
				SP_MsgDecoder * decoder = session->getRequest()->getMsgDecoder();
				if( SP_MsgDecoder::eOK == decoder->decode( session->getInBuffer() ) ) {
					SP_EventHelper::doWork( session );
				}
			}
			addEvent( session, EV_READ, -1 );
		} else {
			int saved = errno;

			if( 0 != errno ) {
				sp_syslog( LOG_WARNING, "session(%d.%d) read error, errno %d, status %d",
						sid.mKey, sid.mSeq, errno, session->getStatus() );
			}

			if( EAGAIN != saved ) {
				if( 0 == session->getRunning() ) {
					SP_EventHelper::doError( session );
				} else {
					sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session error later",
							sid.mKey, sid.mSeq );
					// If this session is running, then onResponse will add write event for this session.
					// It will be processed as write fail at the last. So no need to re-add event here.
				}
			} else {
				addEvent( session, EV_READ, -1 );
			}
		}
	} else {
		if( 0 == session->getRunning() ) {
			SP_EventHelper::doTimeout( session );
		} else {
			sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session timeout later",
					sid.mKey, sid.mSeq );
			// If this session is running, then onResponse will add write event for this session.
			// It will be processed as write fail at the last. So no need to re-add event here.
		}
	}
}

void SP_EventCallback :: onWrite( int fd, short events, void * arg )
{
	SP_Session * session = (SP_Session*)arg;

	session->setWriting( 0 );

	SP_Sid_t sid = session->getSid();

	if( EV_WRITE & events ) {
		int ret = 0;

		if( session->getOutList()->getCount() > 0 ) {
			int len = session->getIOChannel()->transmit( session );
			if( len > 0 ) {
				session->addWrite( len );
				if( session->getOutList()->getCount() > 0 ) {
					// left for next write event
					addEvent( session, EV_WRITE, -1 );
				}
			} else {
				if( EAGAIN != errno ) {
					ret = -1;
					if( 0 == session->getRunning() ) {
						sp_syslog( LOG_NOTICE, "session(%d.%d) write error, errno %d, status %d, count %d",
								sid.mKey, sid.mSeq, errno, session->getStatus(), session->getOutList()->getCount() );
						SP_EventHelper::doError( session );
					} else {
						sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session error later, errno [%d]",
								sid.mKey, sid.mSeq, errno );
						// If this session is running, then onResponse will add write event for this session.
						// It will be processed as write fail at the last. So no need to re-add event here.
					}
				} else {
					addEvent( session, EV_WRITE, -1 );
				}
			}
		}

		if( 0 == ret && session->getOutList()->getCount() <= 0 ) {
			if( SP_Session::eExit == session->getStatus() ) {
				ret = -1;
				if( 0 == session->getRunning() ) {
					sp_syslog( LOG_DEBUG, "session(%d.%d) normal exit", sid.mKey, sid.mSeq );
					SP_EventHelper::doClose( session );
				} else {
					sp_syslog( LOG_NOTICE, "session(%d.%d) busy, terminate session later",
							sid.mKey, sid.mSeq );
					// If this session is running, then onResponse will add write event for this session.
					// It will be processed as write fail at the last. So no need to re-add event here.
				}
			}
		}

		if( 0 == ret ) {
			if( 0 == session->getRunning() ) {
				SP_MsgDecoder * decoder = session->getRequest()->getMsgDecoder();
				if( SP_MsgDecoder::eOK == decoder->decode( session->getInBuffer() ) ) {
					SP_EventHelper::doWork( session );
				}
			} else {
				// If this session is running, then onResponse will add write event for this session.
				// So no need to add write event here.
			}
		}
	} else {
		if( 0 == session->getRunning() ) {
			SP_EventHelper::doTimeout( session );
		} else {
			sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session timeout later",
					sid.mKey, sid.mSeq );
			// If this session is running, then onResponse will add write event for this session.
			// It will be processed as write fail at the last. So no need to re-add event here.
		}
	}
}

void SP_EventCallback :: onResponse( void * queueData, void * arg )
{
	SP_Response * response = (SP_Response*)queueData;
	SP_EventArg * eventArg = (SP_EventArg*)arg;
	SP_SessionManager * manager = eventArg->getSessionManager();

	SP_Sid_t fromSid = response->getFromSid();
	uint16_t seq = 0;

	if( ! SP_EventHelper::isSystemSid( &fromSid ) ) {
		SP_Session * session = manager->get( fromSid.mKey, &seq );
		if( seq == fromSid.mSeq && NULL != session ) {
			if( SP_Session::eWouldExit == session->getStatus() ) {
				session->setStatus( SP_Session::eExit );
			}

			if( SP_Session::eNormal != session->getStatus() ) {
				event_del( session->getReadEvent() );
			}

			// always add a write event for sender, 
			// so the pending input can be processed in onWrite
			addEvent( session, EV_WRITE, -1 );
			addEvent( session, EV_READ, -1 );
		} else {
			sp_syslog( LOG_WARNING, "session(%d.%d) invalid, unknown FROM",
					fromSid.mKey, fromSid.mSeq );
		}
	}

	for( SP_Message * msg = response->takeMessage();
			NULL != msg; msg = response->takeMessage() ) {

		SP_SidList * sidList = msg->getToList();

		if( msg->getTotalSize() > 0 ) {
			for( int i = sidList->getCount() - 1; i >= 0; i-- ) {
				SP_Sid_t sid = sidList->get( i );
				SP_Session * session = manager->get( sid.mKey, &seq );
				if( seq == sid.mSeq && NULL != session ) {
					if( 0 != memcmp( &fromSid, &sid, sizeof( sid ) )
							&& SP_Session::eExit == session->getStatus() ) {
						sidList->take( i );
						msg->getFailure()->add( sid );
						sp_syslog( LOG_WARNING, "session(%d.%d) would exit, invalid TO", sid.mKey, sid.mSeq );
					} else {
						session->getOutList()->append( msg );
						addEvent( session, EV_WRITE, -1 );
					}
				} else {
					sidList->take( i );
					msg->getFailure()->add( sid );
					sp_syslog( LOG_WARNING, "session(%d.%d) invalid, unknown TO", sid.mKey, sid.mSeq );
				}
			}
		} else {
			for( ; sidList->getCount() > 0; ) {
				msg->getFailure()->add( sidList->take( SP_ArrayList::LAST_INDEX ) );
			}
		}

		if( msg->getToList()->getCount() <= 0 ) {
			SP_EventHelper::doCompletion( eventArg, msg );
		}
	}

	for( int i = 0; i < response->getToCloseList()->getCount(); i++ ) {
		SP_Sid_t sid = response->getToCloseList()->get( i );
		SP_Session * session = manager->get( sid.mKey, &seq );
		if( seq == sid.mSeq && NULL != session ) {
			session->setStatus( SP_Session::eExit );
			addEvent( session, EV_WRITE, -1 );
		} else {
			sp_syslog( LOG_WARNING, "session(%d.%d) invalid, unknown CLOSE", sid.mKey, sid.mSeq );
		}
	}

	delete response;
}

void SP_EventCallback :: addEvent( SP_Session * session, short events, int fd )
{
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();

	if( ( events & EV_WRITE ) && 0 == session->getWriting() ) {
		session->setWriting( 1 );

		if( fd < 0 ) fd = EVENT_FD( session->getWriteEvent() );

		event_set( session->getWriteEvent(), fd, events, onWrite, session );
		event_base_set( eventArg->getEventBase(), session->getWriteEvent() );

		struct timeval timeout;
		memset( &timeout, 0, sizeof( timeout ) );
		timeout.tv_sec = eventArg->getTimeout();
		event_add( session->getWriteEvent(), &timeout );
	}

	if( events & EV_READ && 0 == session->getReading() ) {
		session->setReading( 1 );

		if( fd < 0 ) fd = EVENT_FD( session->getWriteEvent() );

		event_set( session->getReadEvent(), fd, events, onRead, session );
		event_base_set( eventArg->getEventBase(), session->getReadEvent() );

		struct timeval timeout;
		memset( &timeout, 0, sizeof( timeout ) );
		timeout.tv_sec = eventArg->getTimeout();
		event_add( session->getReadEvent(), &timeout );
	}
}

//-------------------------------------------------------------------

int SP_EventHelper :: isSystemSid( SP_Sid_t * sid )
{
	return ( sid->mKey == SP_Sid_t::eTimerKey && sid->mSeq == SP_Sid_t::eTimerSeq )
			|| ( sid->mKey == SP_Sid_t::ePushKey && sid->mSeq == SP_Sid_t::ePushSeq );
}

void SP_EventHelper :: doWork( SP_Session * session )
{
	if( SP_Session::eNormal == session->getStatus() ) {
		session->setRunning( 1 );
		SP_EventArg * eventArg = (SP_EventArg*)session->getArg();
		eventArg->getInputResultQueue()->push( new SP_SimpleTask( worker, session, 1 ) );
	} else {
		SP_Sid_t sid = session->getSid();

		char buffer[ 16 ] = { 0 };
		session->getInBuffer()->take( buffer, sizeof( buffer ) );
		sp_syslog( LOG_WARNING, "session(%d.%d) status is %d, ignore [%s...] (%dB)",
			sid.mKey, sid.mSeq, session->getStatus(), buffer, session->getInBuffer()->getSize() );
		session->getInBuffer()->reset();
	}
}

void SP_EventHelper :: worker( void * arg )
{
	SP_Session * session = (SP_Session*)arg;
	SP_Handler * handler = session->getHandler();
	SP_EventArg * eventArg = (SP_EventArg *)session->getArg();

	SP_Response * response = new SP_Response( session->getSid() );
	if( 0 != handler->handle( session->getRequest(), response ) ) {
		session->setStatus( SP_Session::eWouldExit );
	}

	session->setRunning( 0 );

	msgqueue_push( (struct event_msgqueue*)eventArg->getResponseQueue(), response );
}

void SP_EventHelper :: doError( SP_Session * session )
{
	SP_EventArg * eventArg = (SP_EventArg *)session->getArg();

	event_del( session->getWriteEvent() );
	event_del( session->getReadEvent() );

	SP_Sid_t sid = session->getSid();

	SP_ArrayList * outList = session->getOutList();
	for( ; outList->getCount() > 0; ) {
		SP_Message * msg = ( SP_Message * ) outList->takeItem( SP_ArrayList::LAST_INDEX );

		int index = msg->getToList()->find( sid );
		if( index >= 0 ) msg->getToList()->take( index );
		msg->getFailure()->add( sid );

		if( msg->getToList()->getCount() <= 0 ) {
			doCompletion( eventArg, msg );
		}
	}

	// remove session from SessionManager, onResponse will ignore this session
	eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( error, session, 1 ) );
}

void SP_EventHelper :: error( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();

	SP_Sid_t sid = session->getSid();

	SP_Response * response = new SP_Response( sid );
	session->getHandler()->error( response );

	msgqueue_push( (struct event_msgqueue*)eventArg->getResponseQueue(), response );

	sp_syslog( LOG_WARNING, "session(%d.%d) error, r %d(%d), w %d(%d), i %d, o %d, s %d(%d)",
			sid.mKey, sid.mSeq, session->getTotalRead(), session->getReading(),
			session->getTotalWrite(), session->getWriting(),
			session->getInBuffer()->getSize(), session->getOutList()->getCount(),
			eventArg->getSessionManager()->getCount(), eventArg->getSessionManager()->getFreeCount() );

	// onResponse will ignore this session, so it's safe to destroy session here
	session->getHandler()->close();
	sp_close( EVENT_FD( session->getWriteEvent() ) );
	delete session;
}

void SP_EventHelper :: doTimeout( SP_Session * session )
{
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();

	event_del( session->getWriteEvent() );
	event_del( session->getReadEvent() );

	SP_Sid_t sid = session->getSid();

	SP_ArrayList * outList = session->getOutList();
	for( ; outList->getCount() > 0; ) {
		SP_Message * msg = ( SP_Message * ) outList->takeItem( SP_ArrayList::LAST_INDEX );

		int index = msg->getToList()->find( sid );
		if( index >= 0 ) msg->getToList()->take( index );
		msg->getFailure()->add( sid );

		if( msg->getToList()->getCount() <= 0 ) {
			doCompletion( eventArg, msg );
		}
	}

	// remove session from SessionManager, onResponse will ignore this session
	eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( timeout, session, 1 ) );
}

void SP_EventHelper :: timeout( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();

	SP_Sid_t sid = session->getSid();

	SP_Response * response = new SP_Response( sid );
	session->getHandler()->timeout( response );
	msgqueue_push( (struct event_msgqueue*)eventArg->getResponseQueue(), response );

	sp_syslog( LOG_WARNING, "session(%d.%d) timeout, r %d(%d), w %d(%d), i %d, o %d, s %d(%d)",
			sid.mKey, sid.mSeq, session->getTotalRead(), session->getReading(),
			session->getTotalWrite(), session->getWriting(),
			session->getInBuffer()->getSize(), session->getOutList()->getCount(),
			eventArg->getSessionManager()->getCount(), eventArg->getSessionManager()->getFreeCount() );

	// onResponse will ignore this session, so it's safe to destroy session here
	session->getHandler()->close();
	sp_close( EVENT_FD( session->getWriteEvent() ) );
	delete session;
}

void SP_EventHelper :: doClose( SP_Session * session )
{
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();

	event_del( session->getWriteEvent() );
	event_del( session->getReadEvent() );

	SP_Sid_t sid = session->getSid();

	eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( myclose, session, 1 ) );
}

void SP_EventHelper :: myclose( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();
	SP_Sid_t sid = session->getSid();

	sp_syslog( LOG_DEBUG, "session(%d.%d) close, r %d(%d), w %d(%d), i %d, o %d, s %d(%d)",
			sid.mKey, sid.mSeq, session->getTotalRead(), session->getReading(),
			session->getTotalWrite(), session->getWriting(),
			session->getInBuffer()->getSize(), session->getOutList()->getCount(),
			eventArg->getSessionManager()->getCount(), eventArg->getSessionManager()->getFreeCount() );

	session->getHandler()->close();
	sp_close( EVENT_FD( session->getWriteEvent() ) );
	delete session;
}

void SP_EventHelper :: doStart( SP_Session * session )
{
	session->setRunning( 1 );
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();
	eventArg->getInputResultQueue()->push( new SP_SimpleTask( start, session, 1 ) );
}

void SP_EventHelper :: start( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();

	SP_IOChannel * ioChannel = session->getIOChannel();

	int initRet = ioChannel->init( EVENT_FD( session->getWriteEvent() ) );

	// always call SP_Handler::start
	SP_Response * response = new SP_Response( session->getSid() );
	int startRet = session->getHandler()->start( session->getRequest(), response );

	int status = SP_Session::eWouldExit;

	if( 0 == initRet ) {
		if( 0 == startRet ) status = SP_Session::eNormal;
	} else {
		delete response;
		// make an empty response
		response = new SP_Response( session->getSid() );
	}

	session->setStatus( status );
	session->setRunning( 0 );
	msgqueue_push( (struct event_msgqueue*)eventArg->getResponseQueue(), response );
}

void SP_EventHelper :: doCompletion( SP_EventArg * eventArg, SP_Message * msg )
{
	eventArg->getOutputResultQueue()->push( msg );
}

