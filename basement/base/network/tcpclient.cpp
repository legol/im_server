#include "tcpclient.h"

TObjectPool< CTcpClient >	g_objPool_TcpClient;

CTcpClient::CTcpClient( boost::shared_ptr< CEpollEvent > pEpollEvent )
{
	m_pEpollEvent = pEpollEvent;
	m_bAutoReconnect    = false;
	m_tmReconnectTimeout  = 0;
	m_tmLastTimeReconnect = 0;
	m_nConnCount = 0;
	m_nCurrentConnCount = 0;

	m_u16Port = 0;

	m_pCallback_IEpollEventCallback = boost::shared_ptr< CEpollEventCallbackObj< CTcpClient > >( new CEpollEventCallbackObj< CTcpClient >( this ) );
	m_pCallback_IEpollEventCallback->Hook_OnStart( &CTcpClient::OnStart );
	m_pCallback_IEpollEventCallback->Hook_OnIdle( &CTcpClient::OnIdle );
	m_pCallback_IEpollEventCallback->Hook_OnEnd( &CTcpClient::OnEnd );
	m_pCallback_IEpollEventCallback->Hook_OnEpollIn( &CTcpClient::OnEpollIn );
	m_pCallback_IEpollEventCallback->Hook_OnEpollOut( &CTcpClient::OnEpollOut );
	m_pCallback_IEpollEventCallback->Hook_OnEpollErr( &CTcpClient::OnEpollErr );
	m_pCallback_IEpollEventCallback->Hook_OnSendPacket( &CTcpClient::OnSendPacket );
}

CTcpClient::~CTcpClient()
{
	if (m_pEpollEvent)
	{
		m_pEpollEvent->Detach( m_pCallback_IEpollEventCallback );
	}
	m_pCallback_IEpollEventCallback->UnhookAll();

	std::cout << "tcp client free" << std::endl;
}

CTcpClient::CTcpClient()
{
	m_bAutoReconnect    = false;
	m_tmReconnectTimeout  = 0;
	m_tmLastTimeReconnect = 0;
	m_nConnCount = 0;
	m_nCurrentConnCount = 0;

	m_u16Port = 0;

	m_pCallback_IEpollEventCallback = boost::shared_ptr< CEpollEventCallbackObj< CTcpClient > >( new CEpollEventCallbackObj< CTcpClient >( this ) );
	m_pCallback_IEpollEventCallback->Hook_OnStart( &CTcpClient::OnStart );
	m_pCallback_IEpollEventCallback->Hook_OnIdle( &CTcpClient::OnIdle );
	m_pCallback_IEpollEventCallback->Hook_OnEnd( &CTcpClient::OnEnd );
	m_pCallback_IEpollEventCallback->Hook_OnEpollIn( &CTcpClient::OnEpollIn );
	m_pCallback_IEpollEventCallback->Hook_OnEpollOut( &CTcpClient::OnEpollOut );
	m_pCallback_IEpollEventCallback->Hook_OnEpollErr( &CTcpClient::OnEpollErr );
	m_pCallback_IEpollEventCallback->Hook_OnSendPacket( &CTcpClient::OnSendPacket );
}

int32_t CTcpClient::OnStart(void *pThis)
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	time_t	tmCurrent;

	tmCurrent = GetTimeStamp();
	
	if( true == m_strIP.empty() )
	{
		return 0;
	}

	if( m_nCurrentConnCount < m_nConnCount )
	{
		int32_t	fd = 0;
		for( int32_t nIdx = m_nCurrentConnCount; nIdx < m_nConnCount; nIdx++ )
		{
			Connect( m_strIP, m_u16Port, &fd );
		}
	}

	m_tmLastTimeReconnect = tmCurrent;

	if ( m_pTcpEventCallback )
	{
		m_pTcpEventCallback->OnStart( this );
	}
	return 0;
}

void CTcpClient::OnIdle(void *pThis)
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	time_t	tmCurrent;

	tmCurrent = GetTimeStamp();
	if ( tmCurrent - m_tmLastTimeReconnect > m_tmReconnectTimeout )
	{
		Reconnect();
		m_tmLastTimeReconnect = tmCurrent;
	}
}

void CTcpClient::OnEpollIn( int fd, void *pThis )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	if( m_pTcpEventCallback )
	{
		if ( -1 == m_pTcpEventCallback->OnDataIn( fd, this ) )
		{
			Close( fd );
		}
	}
}

void CTcpClient::OnEpollOut( int fd, void *pThis )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	CMapConnContext::iterator itr;

	itr = m_mapConnContext.find( fd );
	if( itr != m_mapConnContext.end() )
	{
		if( CONN_STATUS_NOT_CONNECTED == itr->second )
		{
			if( m_pTcpEventCallback.get() != NULL )
			{
				sockaddr_in	addr;
				addr.sin_family = AF_INET;
				inet_aton( m_strIP.c_str(), ( in_addr* )( &(addr.sin_addr.s_addr) ) );
				addr.sin_port = m_u16Port;

				if ( -1 == m_pTcpEventCallback->OnConnected( fd, addr, this ) )
				{
					Close( fd );
					return;
				}
			}

			itr->second = CONN_STATUS_CONNECTED;
			m_nCurrentConnCount++;
		}
	}

	if( m_pTcpEventCallback )
	{
		m_pTcpEventCallback->OnDataOut( fd, this );
	}

	return;	
}

void CTcpClient::OnSendPacket( int32_t fd, void *pThis )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	if( m_pTcpEventCallback.get() != NULL )
	{
		if( m_pTcpEventCallback->OnSendPacket( fd, this ) == -1 )
		{
			Close( fd );
		}
	}
}


void CTcpClient::OnEpollErr( int fd, void *pThis )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	CMapConnContext::iterator itr;

	itr = m_mapConnContext.find( fd );
	if( itr != m_mapConnContext.end() )
	{
		if( itr->second == CONN_STATUS_NOT_CONNECTED )
		{
			if( m_pTcpEventCallback )
			{
				m_pTcpEventCallback->OnConnectionRefused( fd, this );
				Close( fd );
				return;
			}
		}
		else
		{
			Close( fd );
		}
	}
}

void CTcpClient::OnEnd(void *pThis)
{
}

int32_t	CTcpClient::Init( boost::shared_ptr< CEpollEvent > pEpollEvent, u_int32_t uMaxFd, boost::shared_ptr< ITcpEventCallback > pTcpEventCallback, 
		const std::string &strIP, uint16_t u16Port, bool bAutoReconnect, time_t tmReconnectTimeout, int32_t nConnCount )
{
	if( pEpollEvent.get() == NULL )
	{
		if ( NULL == m_pEpollEvent.get() )
		{
			return -1;
		}
	}
	else
	{
		m_pEpollEvent = pEpollEvent;
	}

	if ( NULL == pTcpEventCallback.get() )
	{
		return -1;
	}
	m_pTcpEventCallback = pTcpEventCallback;

	if( m_pEpollEvent->Init( uMaxFd ) != 0 )
	{
		return -1;
	}

	if( m_pEpollEvent->Attach( m_pCallback_IEpollEventCallback ) != 0 )
	{
		return -1;
	}

	m_mapConnContext.clear();
	m_nCurrentConnCount = 0;

	m_strIP = strIP;
	m_u16Port = u16Port;
	m_bAutoReconnect = bAutoReconnect;
	m_tmReconnectTimeout = tmReconnectTimeout;
	m_nConnCount = nConnCount;

	return 0;
}

int32_t CTcpClient::Close( int32_t fd )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	CMapConnContext::iterator itr;

	itr = m_mapConnContext.find( fd );
	if( itr != m_mapConnContext.end() )
	{
		if( itr->second == CONN_STATUS_CONNECTED )
		{
			m_nCurrentConnCount--;
		}

		m_mapConnContext.erase( fd );
	}

	if( m_pEpollEvent )
	{
		m_pEpollEvent->Close( fd );
	}

	if( m_pTcpEventCallback )
	{
		// 在这之后，有可能这个 tcpclient 已经被释放了，所以不能再操作成员变量，切记！！
		m_pTcpEventCallback->OnClose( fd, this );
	}

	return 0;
}

int32_t CTcpClient::Connect( const std::string &strIP, uint16_t u16Port, int32_t *pfd  )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	int32_t                 fd;
	EConnectionStatus       eConnStatus = CONN_STATUS_NOT_CONNECTED;

	if( NULL == m_pEpollEvent.get() || NULL == pfd )
	{
		return -1;
	}

	*pfd = -1;
	fd = socket(AF_INET, SOCK_STREAM,0) ;
	if (-1 == fd)
	{
		return -1;
	}

	if( m_pTcpEventCallback )
	{
		m_pTcpEventCallback->OnNewSocket( fd, this );
	}

	CNetworkUtil::SetNonBlockSocket( fd );
	if( m_pEpollEvent->RegisterEvent( fd, EPOLLIN | EPOLLOUT | EPOLLET, m_pCallback_IEpollEventCallback ) == -1 )
	{
		Close(fd);
		return -1 ;
	}

	sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	inet_aton( strIP.c_str(), ( in_addr* )( &( addr.sin_addr.s_addr ) ) );
	addr.sin_port = htons( u16Port );

	int conn_r = ::connect(fd, (sockaddr*)&addr, sizeof(addr)) ;
	int e = errno ;
	if ( -1 == conn_r )
	{
		if (e != EINPROGRESS)
		{
			perror( "connect error:" );
			Close(fd);
			return -1;
		}
		else
		{
			// do nothing
		}
	}
	else
	{
		eConnStatus = CONN_STATUS_CONNECTED;
		m_nCurrentConnCount++;
		if( m_pTcpEventCallback )
		{
			if( m_pTcpEventCallback->OnConnected( fd, addr, this ) == -1 )
			{
				Close( fd );
				return -1;
			}
		}
	}

	m_mapConnContext[ fd ] = eConnStatus;
	*pfd = fd;

	return 0;
}

time_t CTcpClient::GetTimeStamp()
{
	timespec t ;
	clock_gettime(CLOCK_REALTIME, &t) ;
	return (time_t) ( ((time_t)t.tv_sec) * 1000UL + (time_t)t.tv_nsec / 1000000 ) ;
}

int32_t	CTcpClient::Reconnect()
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	if( false == m_bAutoReconnect )
	{
		return 0;
	}

	if( true == m_strIP.empty() )
	{
		return 0;
	}

	if( m_nCurrentConnCount < m_nConnCount )
	{
		int32_t	fd = 0;
		for( int32_t nIdx = m_nCurrentConnCount; nIdx < m_nConnCount; nIdx++ )
		{
			Connect( m_strIP, m_u16Port, &fd );
		}
	}

	return 0;
}

// 需要加锁
// 这个函数会被多个线程调用,因为发包是在不同的线程被调用
// 获得所有的已经连接的 fd
int32_t	CTcpClient::GetConnectedFd( std::vector< int32_t > *pVecFd )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockClient );

	for( CMapConnContext::iterator itr = m_mapConnContext.begin(); itr != m_mapConnContext.end(); itr++ )
	{
		if( itr->second == CONN_STATUS_CONNECTED )
		{
			pVecFd->push_back( itr->first );
		}
	}

	return 0;
}

std::string CTcpClient::GetServerIP()
{
	return m_strIP;
}

uint16_t CTcpClient::GetServerPort()
{
	return m_u16Port;
}

