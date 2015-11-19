#include "server_thread.h"
#include "epoll_thread.h"


CServerThread::CServerThread()
{
	m_bSingleThread = false;
}

CServerThread::~CServerThread()
{
	if( m_bSingleThread == false )
	{
		if( m_pEpollEvent.get() != NULL )
		{
			m_pEpollEvent->Stop();
			sleep( 1 );
		}
	}
}

int32_t	CServerThread::Create( boost::shared_ptr< CPacketLayer > pPacketLayer, const std::string &strIP, u_int16_t u16Port,
		bool bSingleThread )
{
	m_bSingleThread = bSingleThread;
	if( m_bSingleThread == false )
	{
		m_pEpollEvent = boost::shared_ptr< CEpollEvent >( new (std::nothrow) CEpollEvent() );
		if( m_pEpollEvent.get() == NULL )
		{
			return -1;
		}
	}
	else
	{
		if( g_pEpollThread.get() == NULL )
		{
			g_pEpollThread = boost::shared_ptr< CEpollThread >( new (std::nothrow) CEpollThread() );
			if( g_pEpollThread.get() == NULL )
			{
				return -1;
			}

			if( g_pEpollThread->Create() != 0 )
			{
				return -1;
			}
		}

		m_pEpollEvent = g_pEpollThread->m_pEpollEvent;
	}

	m_pTcpServer = boost::shared_ptr< CTcpServer >( new (std::nothrow) CTcpServer( ) );
	if( m_pTcpServer.get() == NULL )
	{
		return -1;
	}

	if( m_pTcpServer->Init( m_pEpollEvent, strIP, u16Port, 1000000, pPacketLayer->GetTcpEventCallback() ) != 0 )
	{
		return -1;
	}

	pPacketLayer->m_bIsClient = false;
	m_pPacketLayer = pPacketLayer;

	if( m_bSingleThread == false )
	{
		return CThreadModel::Create();
	}
	else
	{
		return 0;
	}
}

int32_t CServerThread::Stop()
{
	if( m_bSingleThread == false )
	{
		if( m_pEpollEvent )
		{
			m_pEpollEvent->Stop(); 
			sleep( 1 );
		}

		return CThreadModel::Stop();
	}
	else
	{
		if( g_pEpollThread )
		{
			return g_pEpollThread->Stop();
		}

		return -1;

	}
}

int32_t	CServerThread::RunOnce()
{
	if( m_bSingleThread == false )
	{
		if( m_pEpollEvent )
		{
			if( 0 != m_pEpollEvent->Run() )
			{
				std::cout << __FILE__ << ":" << __LINE__ << " " << __FUNCTION__ << "() error!" << std::endl;
				return -1;
			}
		}

		return 0;
	}
	else
	{
		// 单线程模式不应该进入这里
		return -1;
	}

	return -1;
}

int32_t	CServerThread::SendPacket( int32_t fd, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId )
{
	if( m_pPacketLayer.get() == NULL || pchPacket.get() == NULL )
	{
		return -1;
	}

	return m_pPacketLayer->SendPacket( fd, pchPacket, uPacketLen, false, &uPacketId, NULL );
}

int32_t CServerThread::Run( void *pArg )
{
	if( m_bSingleThread == false )
	{
		return CThreadModel::Run( pArg );
	}
	else
	{
		if( g_pEpollThread )
		{
			return g_pEpollThread->Run( pArg );
		}
	}

	return -1;
}

int32_t CServerThread::Join()
{
	if( m_bSingleThread == false )
	{
		return CThreadModel::Join();
	}
	else
	{
		if( g_pEpollThread )
		{
			return g_pEpollThread->Join();
		}
	}

	return -1;
}

int32_t	CServerThread::IsRunning( bool *pbRunning )
{
	if( m_bSingleThread == false )
	{
		return CThreadModel::IsRunning( pbRunning );
	}
	else
	{
		if( g_pEpollThread )
		{
			return g_pEpollThread->IsRunning( pbRunning );
		}
	}

	return -1;
}


