#include "client_thread.h"
#include "epoll_thread.h"


CClientThread::CClientThread()
{
	m_bSingleThread = false;
}

CClientThread::~CClientThread()
{
	if( m_bSingleThread == false )
	{
		if( m_pEpollEvent.get() != NULL )
		{
			m_pEpollEvent->Stop();
		}
	}
}

int32_t	CClientThread::Create( boost::shared_ptr< CPacketLayer > pPacketLayer, const CVecClientInfo &vecClientInfo,
		bool bSingleThread )
{
	boost::shared_ptr< CTcpClient >	pTcpClient;
	char						strIpPort[ 100 ] = {0};

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

	for( CVecClientInfo::const_iterator itr = vecClientInfo.begin(); itr != vecClientInfo.end(); itr++ )
	{
		pTcpClient = boost::shared_ptr< CTcpClient >( new (std::nothrow) CTcpClient( m_pEpollEvent ) );
		if( pTcpClient.get() == NULL )
		{
			m_mapClient.clear();
			return -1;
		}

		pTcpClient->Init( m_pEpollEvent, 1000000, pPacketLayer->GetTcpEventCallback(), 
						( *itr ).strIP, ( *itr ).u16Port, 
						( *itr ).bAutoReconnect, ( *itr ).uTimeoutInterval, ( *itr ).uConnCount );

		memset( strIpPort, 0, sizeof( strIpPort ) );
		snprintf( strIpPort, 99, "%s:%u", ( *itr ).strIP.c_str(), ( *itr ).u16Port );
		m_mapClient[ strIpPort ] = pTcpClient;

		pTcpClient.reset();
	}

	pPacketLayer->m_bIsClient = true;
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

int32_t CClientThread::Stop()
{
	if( m_bSingleThread == false )
	{
		if( m_pEpollEvent )
		{
			m_pEpollEvent->Stop(); 
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

int32_t	CClientThread::RunOnce()
{
	if( m_bSingleThread == false )
	{
		if( m_pEpollEvent )
		{
			return m_pEpollEvent->Run();
		}
	}
	else
	{
		// 单线程模式不应该进入这里
		return -1;
	}

	return -1;
}

int32_t	CClientThread::GetTcpClient( const std::string &strIP, u_int16_t u16Port, boost::shared_ptr< CTcpClient > *ppTcpClient )
{
	char					strIpPort[ 100 ] = { 0 };
	CMapClient::iterator	itr;

	if( ppTcpClient == NULL )
	{
		return -1;
	}
	( *ppTcpClient ).reset();

	// todo：这步操作会耗时
	snprintf( strIpPort, 99, "%s:%u", strIP.c_str(), u16Port );

	itr = m_mapClient.find( strIpPort );
	if( itr == m_mapClient.end() )
	{
		return -1;
	}

	*ppTcpClient = itr->second;

	return 0;
}

int32_t	CClientThread::SendPacket( const std::string &strIP, u_int16_t u16Port, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, 
		bool bNeedResp, u_int32_t *puPacketId, void *pvParam )
{
	boost::shared_ptr< CTcpClient >	pTcpClient;
	std::vector< int32_t >			vecFd;
	u_int32_t						uIdx = 0;
	int32_t							fd;

	if( m_pPacketLayer.get() == NULL || pchPacket.get() == NULL )
	{
		return -1;
	}

	GetTcpClient( strIP, u16Port, &pTcpClient );
	if( pTcpClient.get() == NULL )
	{
		return -1;
	}

	pTcpClient->GetConnectedFd( &vecFd );

	if( vecFd.size() > 0 )
	{
		// 随机挑选一个连接
		uIdx = ( u_int32_t )( ( ( double )rand() / ( double )RAND_MAX ) * vecFd.size() );
		fd = vecFd[ uIdx ];

		return m_pPacketLayer->SendPacket( fd, pchPacket, uPacketLen, bNeedResp, puPacketId, pvParam );
	}
	else
	{
		return -1;
	}
}

int32_t CClientThread::Run( void *pArg )
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

int32_t CClientThread::Join()
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

int32_t	CClientThread::IsRunning( bool *pbRunning )
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


