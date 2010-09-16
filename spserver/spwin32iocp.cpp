/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <assert.h>
#include <time.h>

#include "spwin32iocp.hpp"

#include "spsession.hpp"
#include "spbuffer.hpp"
#include "spmsgdecoder.hpp"
#include "sprequest.hpp"
#include "sputils.hpp"
#include "sphandler.hpp"
#include "spexecutor.hpp"
#include "spioutils.hpp"
#include "spmsgblock.hpp"
#include "spwin32buffer.hpp"
#include "spiochannel.hpp"

BOOL SP_IocpEventCallback :: addSession( SP_IocpEventArg * eventArg, HANDLE client, SP_Session * session )
{
	BOOL ret = TRUE;

	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)malloc( sizeof( SP_IocpSession_t ) );
	if( NULL == iocpSession ) {
		sp_syslog( LOG_ERR, "malloc fail, errno %d", GetLastError() );
		ret = FALSE;
	}

	DWORD completionKey = 0;
	SP_Sid_t sid = session->getSid();
	assert( sizeof( completionKey ) == sizeof( SP_Sid_t ) );
	memcpy( &completionKey, &sid, sizeof( completionKey ) );

	if( ret ) {
		memset( iocpSession, 0, sizeof( SP_IocpSession_t ) );
		iocpSession->mRecvEvent.mHeapIndex = -1;
		iocpSession->mSendEvent.mHeapIndex = -1;
		iocpSession->mRecvEvent.mType = SP_IocpEvent_t::eEventRecv;
		iocpSession->mSendEvent.mType = SP_IocpEvent_t::eEventSend;

		iocpSession->mHandle = client;
		iocpSession->mSession = session;
		iocpSession->mEventArg = eventArg;
		session->setArg( iocpSession );

		if( NULL == CreateIoCompletionPort( client, eventArg->getCompletionPort(), completionKey, 0 ) ) {
			sp_syslog( LOG_ERR, "CreateIoCompletionPort fail, errno %d", WSAGetLastError() );
			ret = FALSE;
		}
	}

	if( ! ret ) {
		sp_close( (SOCKET)client );

		if( NULL != iocpSession ) free( iocpSession );
		session->setArg( NULL );
	}

	return ret;

}

BOOL SP_IocpEventCallback :: addRecv( SP_Session * session )
{
	BOOL ret = TRUE;

	if( 0 == session->getReading() && SP_Session::eNormal == session->getStatus() ) {
		SP_Sid_t sid = session->getSid();

		SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
		SP_IocpEvent_t * recvEvent = &( iocpSession->mRecvEvent );

		const int SP_MAX_RETRY = 5;

		for( int retry = 0; retry < SP_MAX_RETRY; retry++ ) {
			memset( &( recvEvent->mOverlapped ), 0, sizeof( OVERLAPPED ) );
			recvEvent->mType = SP_IocpEvent_t::eEventRecv;
			recvEvent->mWsaBuf.buf = NULL;
			recvEvent->mWsaBuf.len = 0;

			DWORD recvBytes = 0, flags = 0;
			if( SOCKET_ERROR == WSARecv( (SOCKET)iocpSession->mHandle, &(recvEvent->mWsaBuf), 1,
					&recvBytes, &flags, &( recvEvent->mOverlapped ), NULL ) ) {
				int lastError = WSAGetLastError();
				if( ERROR_IO_PENDING != lastError ) {
					sp_syslog( LOG_ERR, "session(%d.%d) WSARecv fail, errno %d, retry %d",
							sid.mKey, sid.mSeq, lastError, retry );
				}

				if( WSAENOBUFS == lastError && retry < SP_MAX_RETRY - 1 ) {
					Sleep( 50 * retry );
					continue;
				} else {
					if( ERROR_IO_PENDING != lastError ) ret = FALSE;
					break;
				}
			} else {
				break;
			}
		}

		if( ret ) {
			iocpSession->mSession->setReading( 1 );

			SP_IocpEventArg * eventArg = iocpSession->mEventArg;

			if( eventArg->getTimeout() > 0 ) {
				sp_gettimeofday( &( recvEvent->mTimeout ), NULL );
				recvEvent->mTimeout.tv_sec += eventArg->getTimeout();
				eventArg->getEventHeap()->push( recvEvent );
			}
		}
	}

	return ret;
}

void SP_IocpEventCallback :: onRecv( SP_IocpSession_t * iocpSession )
{
	SP_IocpEvent_t * recvEvent = &( iocpSession->mRecvEvent );
	SP_Session * session = iocpSession->mSession;
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	eventArg->getEventHeap()->erase( recvEvent );

	session->setReading( 0 );

	int len = session->getIOChannel()->receive( session );

	if( len > 0 ) {
		session->addRead( len );
		if( 0 == session->getRunning() ) {
			SP_IocpEventHelper::doDecodeForWork( session );
		}
		if( ! addRecv( session ) ) {
			if( 0 == session->getRunning() ) {
				SP_IocpEventHelper::doError( session );
			} else {
				sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session error later",
						sid.mKey, sid.mSeq );
			}
		}
	} else if( 0 == len ) {
		if( 0 == session->getRunning() ) {
			SP_IocpEventHelper::doClose( session );
		} else {
			sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session close later",
					sid.mKey, sid.mSeq );
		}
	} else {
		int ret = -1, lastError = WSAGetLastError();

		if( WSAEWOULDBLOCK == lastError && addRecv( session ) ) ret = 0;

		if( 0 != ret ) {
			if( 0 == session->getRunning() ) {
				sp_syslog( LOG_NOTICE, "session(%d.%d) read error, errno %d, status %d",
					sid.mKey, sid.mSeq, lastError, session->getStatus() );
				SP_IocpEventHelper::doError( session );
			} else {
				sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session error later",
						sid.mKey, sid.mSeq );
			}
		}
	}
}

BOOL SP_IocpEventCallback :: addSend( SP_Session * session )
{
	BOOL ret = TRUE;

	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;
	SP_IocpEvent_t * sendEvent = &( iocpSession->mSendEvent );
	SP_Sid_t sid = session->getSid();

	if( 0 == session->getRunning() ) {
		SP_IocpEventHelper::doDecodeForWork( session );
	}

	if( 0 == session->getWriting() ) {

		const int SP_MAX_RETRY = 5;

		for( int retry = 0; retry < SP_MAX_RETRY; retry++ ) {
			memset( &( sendEvent->mOverlapped ), 0, sizeof( OVERLAPPED ) );
			sendEvent->mType = SP_IocpEvent_t::eEventSend;
			sendEvent->mWsaBuf.buf = NULL;
			sendEvent->mWsaBuf.len = 0;

			DWORD sendBytes = 0;

			if( SOCKET_ERROR == WSASend( (SOCKET)iocpSession->mHandle, &( sendEvent->mWsaBuf ), 1,
					&sendBytes, 0,	&( sendEvent->mOverlapped ), NULL ) ) {
				int lastError = WSAGetLastError();
				if( ERROR_IO_PENDING != lastError ) {
					sp_syslog( LOG_ERR, "session(%d.%d) WSASend fail, errno %d, retry %d",
							sid.mKey, sid.mSeq, lastError, retry );
				}

				if( WSAENOBUFS == lastError && retry < SP_MAX_RETRY - 1 ) {
					Sleep( 50 * retry );
					continue;
				} else {
					if( ERROR_IO_PENDING != lastError ) ret = FALSE;
					break;
				}
			} else {
				break;
			}
		}

		if( ret ) {
			if( eventArg->getTimeout() > 0 ) {
				sp_gettimeofday( &( sendEvent->mTimeout ), NULL );
				sendEvent->mTimeout.tv_sec += eventArg->getTimeout();
				eventArg->getEventHeap()->push( sendEvent );
			}

			session->setWriting( 1 );
		}
	}

	return ret;
}

void SP_IocpEventCallback :: onSend( SP_IocpSession_t * iocpSession )
{
	SP_IocpEvent_t * recvEvent = &( iocpSession->mRecvEvent );
	SP_Session * session = iocpSession->mSession;
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	session->setWriting( 0 );

	SP_Sid_t sid = session->getSid();

	int ret = 0;

	if( session->getOutList()->getCount() > 0 ) {
		int len = session->getIOChannel()->transmit( session );
		if( len > 0 ) {
			session->addWrite( len );
			if( session->getOutList()->getCount() > 0 ) {
				if( ! addSend( session ) ) {
					if( 0 == session->getRunning() ) {
						ret = -1;
						SP_IocpEventHelper::doError( session );
					}
				}
			}
		} else {
			ret = -1;

			int lastError = WSAGetLastError();

			if( WSAENOBUFS == lastError && addSend( session ) ) ret = 0;

			if( WSAEWOULDBLOCK == lastError && addSend( session ) ) ret = 0;

			if( 0 != ret ) {
				if( 0 == session->getRunning() ) {
					sp_syslog( LOG_NOTICE, "session(%d.%d) write error, errno %d, status %d, count %d",
							sid.mKey, sid.mSeq, lastError, session->getStatus(), session->getOutList()->getCount() );
					SP_IocpEventHelper::doError( session );
				} else {
					sp_syslog( LOG_NOTICE, "session(%d.%d) busy, process session error later, errno [%d]",
							sid.mKey, sid.mSeq, errno );
				}
			}
		}
	}

	if( 0 == ret && session->getOutList()->getCount() <= 0 ) {
		if( SP_Session::eExit == session->getStatus() ) {
			ret = -1;
			if( 0 == session->getRunning() ) {
				//sp_syslog( LOG_NOTICE, "session(%d.%d) normal exit", sid.mKey, sid.mSeq );
				SP_IocpEventHelper::doClose( session );
			} else {
				sp_syslog( LOG_NOTICE, "session(%d.%d) busy, terminate session later",
						sid.mKey, sid.mSeq );
			}
		}
	}

	if( 0 == ret && 0 == session->getRunning() ) {
		SP_IocpEventHelper::doDecodeForWork( session );
	}
}

BOOL SP_IocpEventCallback :: onAccept( SP_IocpAcceptArg_t * acceptArg )
{
	SP_IocpEventArg * eventArg = acceptArg->mEventArg;

	SP_Sid_t sid;
	sid.mKey = eventArg->getSessionManager()->allocKey( &sid.mSeq );
	assert( sid.mKey > 0 );

	SP_Session * session = new SP_Session( sid );

	int localLen = 0, remoteLen = 0;
	struct sockaddr_in * localAddr = NULL, * remoteAddr = NULL;

	GetAcceptExSockaddrs( acceptArg->mBuffer, 0,
		sizeof( sockaddr_in ) + 16, sizeof( sockaddr_in ) + 16,
		(SOCKADDR**)&localAddr, &localLen, (SOCKADDR**)&remoteAddr, &remoteLen );

	struct sockaddr_in clientAddr;
	memcpy( &clientAddr, remoteAddr, sizeof( clientAddr ) );

	char clientIP[ 32 ] = { 0 };
	SP_IOUtils::inetNtoa( &( clientAddr.sin_addr ), clientIP, sizeof( clientIP ) );
	session->getRequest()->setClientIP( clientIP );
	session->getRequest()->setClientPort( ntohs( clientAddr.sin_port ) );

	session->setHandler( acceptArg->mHandlerFactory->create() );	
	session->setIOChannel( acceptArg->mIOChannelFactory->create() );

	if( addSession( eventArg, acceptArg->mClientSocket, session ) ) {
		eventArg->getSessionManager()->put( sid.mKey, sid.mSeq, session );

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

			addSend( session );
		} else {
			SP_IocpEventHelper::doStart( session );
		}
	} else {
		eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );
		delete session;
	}

	// signal SP_IocpServer::acceptThread to post another AcceptEx
	SetEvent( acceptArg->mAcceptEvent );

	return TRUE;
}

void SP_IocpEventCallback :: onResponse( void * queueData, void * arg )
{
	SP_Response * response = (SP_Response*)queueData;
	SP_IocpEventArg * eventArg = (SP_IocpEventArg*)arg;
	SP_SessionManager * manager = eventArg->getSessionManager();

	SP_Sid_t fromSid = response->getFromSid();
	uint16_t seq = 0;

	if( ! SP_IocpEventHelper::isSystemSid( &fromSid ) ) {
		SP_Session * session = manager->get( fromSid.mKey, &seq );
		if( seq == fromSid.mSeq && NULL != session ) {
			if( SP_Session::eWouldExit == session->getStatus() ) {
				session->setStatus( SP_Session::eExit );
			}

			if( SP_Session::eNormal == session->getStatus() ) {
				if( addRecv( session ) ) {
					if( 0 == session->getRunning() ) {
						SP_IocpEventHelper::doDecodeForWork( session );
					}
				} else {
					if( 0 == session->getRunning() ) {
						SP_IocpEventHelper::doError( session );
					}
				}
			}
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
						if( addSend( session ) ) {
							session->getOutList()->append( msg );
						} else {
							if( 0 == session->getRunning() ) {
								SP_IocpEventHelper::doError( session );
							}
						}
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
			SP_IocpEventHelper::doCompletion( eventArg, msg );
		}
	}

	if( ! SP_IocpEventHelper::isSystemSid( &fromSid ) ) {
		SP_Session * session = manager->get( fromSid.mKey, &seq );
		if( seq == fromSid.mSeq && NULL != session ) {
			if( session->getOutList()->getCount() <= 0 && SP_Session::eExit == session->getStatus() ) {
				if( 0 == session->getRunning() ) {
					SP_IocpEventHelper::doClose( session );
				} else {
				sp_syslog( LOG_NOTICE, "session(%d.%d) busy, terminate session later",
						fromSid.mKey, fromSid.mSeq );
				}
			}
		}
	}

	for( int i = 0; i < response->getToCloseList()->getCount(); i++ ) {
		SP_Sid_t sid = response->getToCloseList()->get( i );
		SP_Session * session = manager->get( sid.mKey, &seq );
		if( seq == sid.mSeq && NULL != session ) {
			session->setStatus( SP_Session::eExit );
			if( !addSend( session ) ) {
				if( 0 == session->getRunning() ) {
					SP_IocpEventHelper::doError( session );
				}
			}
		} else {
			sp_syslog( LOG_WARNING, "session(%d.%d) invalid, unknown CLOSE", sid.mKey, sid.mSeq );
		}
	}

	delete response;
}

void SP_IocpEventCallback :: onTimeout( SP_IocpEventArg * eventArg )
{
	SP_IocpEventHeap * eventHeap = eventArg->getEventHeap();

	if( NULL == eventHeap->top() ) return;
	
	struct timeval curr;
	sp_gettimeofday( &curr, NULL );

	for( ; NULL != eventHeap->top(); ) {
		SP_IocpEvent_t * event = eventHeap->top();
		struct timeval * first = &( event->mTimeout );

		if( ( curr.tv_sec == first->tv_sec && curr.tv_usec >= first->tv_usec )
				||( curr.tv_sec > first->tv_sec ) ) {
			event = eventHeap->pop();

			if( SP_IocpEvent_t::eEventTimer == event->mType ) {
				event->mOnTimer( event );
			} else {
				SP_IocpSession_t * iocpSession = NULL;

				if( SP_IocpEvent_t::eEventRecv == event->mType ) {
					iocpSession = CONTAINING_RECORD( event, SP_IocpSession_t, mRecvEvent );
				} else if( SP_IocpEvent_t::eEventSend == event->mType ) {
					iocpSession = CONTAINING_RECORD( event, SP_IocpSession_t, mSendEvent );
				}

				assert( NULL != iocpSession );

				if( 0 == iocpSession->mSession->getRunning() ) {
					SP_IocpEventHelper::doTimeout( iocpSession->mSession );
				}
			}
		} else {
			break;
		}
	}
}

BOOL SP_IocpEventCallback :: eventLoop( SP_IocpEventArg * eventArg, SP_IocpAcceptArg_t * acceptArg )
{
	DWORD bytesTransferred = 0;
	DWORD completionKey = 0;
	OVERLAPPED * overlapped = NULL;
	HANDLE completionPort = eventArg->getCompletionPort();
	DWORD timeout = SP_IocpEventHelper::timeoutNext( eventArg->getEventHeap() );

	BOOL isSuccess = GetQueuedCompletionStatus( completionPort, &bytesTransferred,
			&completionKey, &overlapped, timeout );
	DWORD lastError = WSAGetLastError();

	SP_Sid_t sid;
	memcpy( &sid, &completionKey, sizeof( completionKey ) );

	SP_IocpSession_t * iocpSession = NULL;
	if( completionKey > 0 ) {
		uint16_t seq = 0;
		SP_Session * session = eventArg->getSessionManager()->get( sid.mKey, &seq );
		if( NULL != session && sid.mSeq == seq ) {
			iocpSession = (SP_IocpSession_t*)session->getArg();
		}
	}

	if( ! isSuccess ) {
		if( eKeyAccept == completionKey ) {
			sp_syslog( LOG_ERR, "accept(%d) fail, errno %d", acceptArg->mClientSocket, lastError );
			sp_close( (SOCKET)acceptArg->mClientSocket );
			// signal SP_IocpServer::acceptThread to post another AcceptEx
			SetEvent( acceptArg->mAcceptEvent );
			return TRUE;
		}

		if( NULL != overlapped ) {
			// process a failed completed I/O request
			// lastError continas the reason for failure

			if( NULL != iocpSession ) {
				if( 0 == iocpSession->mSession->getRunning() ) {
					SP_IocpEventHelper::doClose( iocpSession->mSession );
				}
			}

			if( ERROR_NETNAME_DELETED == lastError // client abort
					|| ERROR_OPERATION_ABORTED == lastError ) {
				return TRUE;
			} else {
				char errmsg[ 512 ] = { 0 };
				spwin32_strerror( lastError, errmsg, sizeof( errmsg ) );
				sp_syslog( LOG_ERR, "GetQueuedCompletionStatus fail, errno %d, %s",
						lastError, errmsg );
				return FALSE;
			}
		} else {
			if( lastError == WAIT_TIMEOUT ) {
				// time-out while waiting for completed I/O request
				onTimeout( eventArg );
			} else {
				// bad call to GQCS, lastError contains the reason for the bad call
			}
		}

		return FALSE;
	}

	if( eKeyAccept == completionKey ) {
		return onAccept( acceptArg );
	} else if( eKeyMsgQueue == completionKey ) {
		SP_IocpMsgQueue * msgQueue = (SP_IocpMsgQueue*)overlapped;
		msgQueue->process();
		return TRUE;
	} else if( eKeyFree == completionKey ) {
		assert( NULL == iocpSession );
		iocpSession = CONTAINING_RECORD( overlapped, SP_IocpSession_t, mFreeEvent );
		delete iocpSession->mSession;
		free( iocpSession );
		return TRUE;
	} else {
		if( NULL == iocpSession ) return TRUE;

		SP_IocpEvent_t * iocpEvent = 
				CONTAINING_RECORD( overlapped, SP_IocpEvent_t, mOverlapped );

		eventArg->getEventHeap()->erase( iocpEvent );

		if( SP_IocpEvent_t::eEventRecv == iocpEvent->mType ) {
			onRecv( iocpSession );
			return TRUE;
		}

		if( SP_IocpEvent_t::eEventSend == iocpEvent->mType ) {
			onSend( iocpSession );
			return TRUE;
		}
	}

	return TRUE;
}

//===================================================================

int SP_IocpEventHelper :: isSystemSid( SP_Sid_t * sid )
{
	return sid->mKey == SP_Sid_t::eTimerKey && sid->mSeq == SP_Sid_t::eTimerSeq;
}

DWORD SP_IocpEventHelper :: timeoutNext( SP_IocpEventHeap * eventHeap )
{
	SP_IocpEvent_t * event = eventHeap->top();

	if( NULL == event ) return INFINITE;

	struct timeval curr;
	sp_gettimeofday( &curr, NULL );

	struct timeval * first = &( event->mTimeout );

	DWORD ret = ( first->tv_sec - curr.tv_sec ) * 1000
		+ ( first->tv_usec - curr.tv_usec ) / 1000;

	if( ret < 0 ) ret = 0;

	return ret;
}

void SP_IocpEventHelper :: doDecodeForWork( SP_Session * session )
{
	SP_MsgDecoder * decoder = session->getRequest()->getMsgDecoder();
	int ret = decoder->decode( session->getInBuffer() );
	if( SP_MsgDecoder::eOK == ret ) {
		doWork( session );
	} else if( SP_MsgDecoder::eMoreData != ret ) {
		doError( session );
	} else {
		assert( ret == SP_MsgDecoder::eMoreData );
	}
}

void SP_IocpEventHelper :: doWork( SP_Session * session )
{
	if( SP_Session::eNormal == session->getStatus() ) {
		session->setRunning( 1 );
		SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
		SP_IocpEventArg * eventArg = iocpSession->mEventArg;
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

void SP_IocpEventHelper :: worker( void * arg )
{
	SP_Session * session = (SP_Session*)arg;
	SP_Handler * handler = session->getHandler();
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Response * response = new SP_Response( session->getSid() );
	if( 0 != handler->handle( session->getRequest(), response ) ) {
		session->setStatus( SP_Session::eWouldExit );
	}

	session->setRunning( 0 );

	eventArg->getResponseQueue()->push( response );
}

void SP_IocpEventHelper :: doClose( SP_Session * session )
{
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	session->setRunning( 1 );

	// remove session from SessionManager, the other threads will ignore this session
	eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );

	eventArg->getEventHeap()->erase( &( iocpSession->mRecvEvent ) );
	eventArg->getEventHeap()->erase( &( iocpSession->mSendEvent ) );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( close, session, 1 ) );
}

void SP_IocpEventHelper :: close( void * arg )
{
	SP_Session * session = (SP_Session*)arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	//sp_syslog( LOG_NOTICE, "session(%d.%d) close, disconnect", sid.mKey, sid.mSeq );

	session->getHandler()->close();

	sp_syslog( LOG_NOTICE, "session(%d.%d) close, r %d(%d), w %d(%d), i %d, o %d, s %d(%d), t %d",
			sid.mKey, sid.mSeq, session->getTotalRead(), session->getReading(),
			session->getTotalWrite(), session->getWriting(),
			session->getInBuffer()->getSize(), session->getOutList()->getCount(),
			eventArg->getSessionManager()->getCount(), eventArg->getSessionManager()->getFreeCount(),
			eventArg->getEventHeap()->getCount() );

	if( ! eventArg->disconnectEx( (SOCKET)iocpSession->mHandle, NULL, 0, 0 ) ) {
		if( ERROR_IO_PENDING != WSAGetLastError () ) {
			sp_syslog( LOG_ERR, "DisconnectEx(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
		}
	}

	if( 0 != sp_close( (SOCKET)iocpSession->mHandle ) ) {
		sp_syslog( LOG_ERR, "close(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
	}

	memset( &( iocpSession->mFreeEvent ), 0, sizeof( OVERLAPPED ) );
	PostQueuedCompletionStatus( eventArg->getCompletionPort(), 0,
			SP_IocpEventCallback::eKeyFree, &( iocpSession->mFreeEvent ) );
}

void SP_IocpEventHelper :: doError( SP_Session * session )
{
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;
	SP_Sid_t sid = session->getSid();

	sp_syslog( LOG_WARNING, "session(%d.%d) error, r %d(%d), w %d(%d), i %d, o %d, s %d(%d), t %d",
			sid.mKey, sid.mSeq, session->getTotalRead(), session->getReading(),
			session->getTotalWrite(), session->getWriting(),
			session->getInBuffer()->getSize(), session->getOutList()->getCount(),
			eventArg->getSessionManager()->getCount(), eventArg->getSessionManager()->getFreeCount(),
			eventArg->getEventHeap()->getCount() );

	session->setRunning( 1 );

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

	// remove session from SessionManager, so the other threads will ignore this session
	eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );

	eventArg->getEventHeap()->erase( &( iocpSession->mRecvEvent ) );
	eventArg->getEventHeap()->erase( &( iocpSession->mSendEvent ) );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( error, session, 1 ) );
}

void SP_IocpEventHelper :: error( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	SP_Response * response = new SP_Response( sid );
	session->getHandler()->error( response );

	eventArg->getResponseQueue()->push( response );

	// the other threads will ignore this session, so it's safe to destroy session here
	session->getHandler()->close();

	if( ! eventArg->disconnectEx( (SOCKET)iocpSession->mHandle, NULL, 0, 0 ) ) {
		if( ERROR_IO_PENDING != WSAGetLastError () ) {
			sp_syslog( LOG_ERR, "DisconnectEx(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
		}
	}

	if( 0 != sp_close( (SOCKET)iocpSession->mHandle ) ) {
		sp_syslog( LOG_ERR, "close(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
	}

	memset( &( iocpSession->mFreeEvent ), 0, sizeof( OVERLAPPED ) );
	PostQueuedCompletionStatus( eventArg->getCompletionPort(), 0,
			SP_IocpEventCallback::eKeyFree, &( iocpSession->mFreeEvent ) );
}

void SP_IocpEventHelper :: doTimeout( SP_Session * session )
{
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;
	SP_Sid_t sid = session->getSid();

	sp_syslog( LOG_WARNING, "session(%d.%d) timeout, r %d(%d), w %d(%d), i %d, o %d, s %d(%d), t %d",
			sid.mKey, sid.mSeq, session->getTotalRead(), session->getReading(),
			session->getTotalWrite(), session->getWriting(),
			session->getInBuffer()->getSize(), session->getOutList()->getCount(),
			eventArg->getSessionManager()->getCount(), eventArg->getSessionManager()->getFreeCount(),
			eventArg->getEventHeap()->getCount() );

	session->setRunning( 1 );

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

	// remove session from SessionManager, the other threads will ignore this session
	eventArg->getSessionManager()->remove( sid.mKey, sid.mSeq );

	eventArg->getEventHeap()->erase( &( iocpSession->mRecvEvent ) );
	eventArg->getEventHeap()->erase( &( iocpSession->mSendEvent ) );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( timeout, session, 1 ) );
}

void SP_IocpEventHelper :: timeout( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	SP_Response * response = new SP_Response( sid );
	session->getHandler()->timeout( response );

	eventArg->getResponseQueue()->push( response );

	// the other threads will ignore this session, so it's safe to destroy session here
	session->getHandler()->close();

	if( ! eventArg->disconnectEx( (SOCKET)iocpSession->mHandle, NULL, 0, 0 ) ) {
		if( ERROR_IO_PENDING != WSAGetLastError () ) {
			sp_syslog( LOG_ERR, "DisconnectEx(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
		}
	}

	if( 0 != sp_close( (SOCKET)iocpSession->mHandle ) ) {
		sp_syslog( LOG_ERR, "close(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
	}

	memset( &( iocpSession->mFreeEvent ), 0, sizeof( OVERLAPPED ) );
	PostQueuedCompletionStatus( eventArg->getCompletionPort(), 0,
			SP_IocpEventCallback::eKeyFree, &( iocpSession->mFreeEvent ) );
}

void SP_IocpEventHelper :: doStart( SP_Session * session )
{
	session->setRunning( 1 );

	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;
	eventArg->getInputResultQueue()->push( new SP_SimpleTask( start, session, 1 ) );
}

void SP_IocpEventHelper :: start( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_IOChannel * ioChannel = session->getIOChannel();

	int initRet = ioChannel->init( (int)iocpSession->mHandle );

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

	eventArg->getResponseQueue()->push( response );
}

void SP_IocpEventHelper :: doCompletion( SP_IocpEventArg * eventArg, SP_Message * msg )
{
	eventArg->getOutputResultQueue()->push( msg );
}
