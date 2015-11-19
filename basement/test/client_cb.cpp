#include "client_cb.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClientPacketCb::CClientPacketCb()
{
}

CClientPacketCb::~CClientPacketCb()
{
}

int32_t	CClientPacketCb::Init( const std::string &strEventSource,
							boost::shared_ptr< CMemPool > pMemPool,
							boost::shared_ptr< CHttpPacketLayer > pPacketLayer,
							boost::shared_ptr< CThreadPool< tagHttpEvent > > pThreadPool,
							boost::shared_ptr< CHandler > pHandler )
{
	if( pMemPool.get() == NULL || pPacketLayer.get() == NULL || pThreadPool.get() == NULL || pHandler.get() == NULL )
	{
		return -1;
	}

	m_pMemPool = pMemPool;
	m_pPacketLayer = pPacketLayer;
	m_pThreadPool = pThreadPool;
	m_pHandler = pHandler;
	m_strEventSource = strEventSource;

	return 0;
}

int32_t CClientPacketCb::OnPacketReceived( int32_t fd, sockaddr_in addr, 
                                    boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, // °üÀ¨ \r\n\r\n
                                    boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                    u_int32_t uPacketId )

{
	if( pchHead.get() == NULL || pchBody.get() == NULL )
	{
		// log error
		return 0;
	}

	if( m_pThreadPool.get() != NULL )
	{
		boost::shared_ptr< tagHttpEvent >	pEvent;

		pEvent = boost::shared_ptr< tagHttpEvent >( new (std::nothrow) tagHttpEvent() );
		if( pEvent == NULL )
		{
			return -1;
		}
	
		pEvent->pPacketLayer = m_pPacketLayer;
		pEvent->strEventSource = m_strEventSource;
		pEvent->eEventType = EVENT_TYPE_PACKET_RECEIVED;
		pEvent->fd = fd;
		pEvent->addr = addr;
		pEvent->uPacketId = uPacketId;
		pEvent->pHttpHead = pchHead;
		pEvent->uHttpHeadLen = uHeadLen;
		pEvent->pHttpBody = pchBody;
		pEvent->uHttpBodyLen = uBodyLen;

		m_pThreadPool->QueueWorkItem( pEvent, m_pHandler );
	}

    return 0;
}

int32_t CClientPacketCb::OnPacketSent( int32_t fd, sockaddr_in addr, 
                                boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, 
                                boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                u_int32_t uPacketId )

{
	if( m_pThreadPool.get() != NULL )
	{
		boost::shared_ptr< tagHttpEvent >	pEvent;

		pEvent = boost::shared_ptr< tagHttpEvent >( new (std::nothrow) tagHttpEvent() );
		if( pEvent == NULL )
		{
			return -1;
		}
	
		pEvent->pPacketLayer = m_pPacketLayer;
		pEvent->strEventSource = m_strEventSource;
		pEvent->eEventType = EVENT_TYPE_PACKET_SENT;
		pEvent->fd = fd;
		pEvent->addr = addr;
		pEvent->uPacketId = uPacketId;
        pEvent->pHttpHead = pchHead;
        pEvent->uHttpHeadLen = uHeadLen;
        pEvent->pHttpBody = pchBody;
        pEvent->uHttpBodyLen = uBodyLen;

		m_pThreadPool->QueueWorkItem( pEvent, m_pHandler );
	}

    return 0;
}

int32_t CClientPacketCb::OnConnected(int32_t fd, sockaddr_in addr)
{
	if( m_pThreadPool.get() != NULL )
	{
		boost::shared_ptr< tagHttpEvent >	pEvent;

		pEvent = boost::shared_ptr< tagHttpEvent >( new (std::nothrow) tagHttpEvent() );
		if( pEvent == NULL )
		{
			return -1;
		}

	
		pEvent->pPacketLayer = m_pPacketLayer;
		pEvent->strEventSource = m_strEventSource;
		pEvent->eEventType = EVENT_TYPE_CONNECTED;
		pEvent->fd = fd;
		pEvent->addr = addr;
		m_pThreadPool->QueueWorkItem( pEvent, m_pHandler );
	}

    return 0;
}

int32_t CClientPacketCb::OnClose(int32_t fd, sockaddr_in addr)
{
	if( m_pThreadPool.get() != NULL )
	{
		boost::shared_ptr< tagHttpEvent >	pEvent;

		pEvent = boost::shared_ptr< tagHttpEvent >( new (std::nothrow) tagHttpEvent() );
		if( pEvent == NULL )
		{
			return -1;
		}
	
		pEvent->pPacketLayer = m_pPacketLayer;
		pEvent->strEventSource = m_strEventSource;
		pEvent->eEventType = EVENT_TYPE_CLOSE;
		pEvent->fd = fd;
		pEvent->addr = addr;
		m_pThreadPool->QueueWorkItem( pEvent, m_pHandler );
	}

    return -1;
}

int32_t CClientPacketCb::OnTimeout( boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, 
                                    boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                    u_int32_t uPacketId )
{
	if( m_pThreadPool.get() != NULL )
	{
		boost::shared_ptr< tagHttpEvent >	pEvent;

		pEvent = boost::shared_ptr< tagHttpEvent >( new (std::nothrow) tagHttpEvent() );
		if( pEvent == NULL )
		{
			return -1;
		}
	
	
		pEvent->pPacketLayer = m_pPacketLayer;
		pEvent->strEventSource = m_strEventSource;
		pEvent->eEventType = EVENT_TYPE_PACKET_TIMEOUT;
		pEvent->uPacketId = uPacketId;
        pEvent->pHttpHead = pchHead;
        pEvent->uHttpHeadLen = uHeadLen;
        pEvent->pHttpBody = pchBody;
        pEvent->uHttpBodyLen = uBodyLen;

		m_pThreadPool->QueueWorkItem( pEvent, m_pHandler );
	}
	
	return 0;
}


