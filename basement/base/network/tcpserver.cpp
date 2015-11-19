#include "tcpserver.h"


CTcpServer::CTcpServer( boost::shared_ptr< CEpollEvent > pEpollEvent )
{
	m_nListenFd = -1;
	m_strIP.clear();
	m_u16Port = 0;
	m_pEpollEvent = pEpollEvent;

	m_pCallback_IEpollEventCallback = boost::shared_ptr< CEpollEventCallbackObj< CTcpServer > >( new CEpollEventCallbackObj< CTcpServer >( this ) );
	m_pCallback_IEpollEventCallback->Hook_OnStart( &CTcpServer::OnStart );
	m_pCallback_IEpollEventCallback->Hook_OnIdle( &CTcpServer::OnIdle );
	m_pCallback_IEpollEventCallback->Hook_OnEnd( &CTcpServer::OnEnd );
	m_pCallback_IEpollEventCallback->Hook_OnEpollIn( &CTcpServer::OnEpollIn );
	m_pCallback_IEpollEventCallback->Hook_OnEpollOut( &CTcpServer::OnEpollOut );
	m_pCallback_IEpollEventCallback->Hook_OnEpollErr( &CTcpServer::OnEpollErr );
	m_pCallback_IEpollEventCallback->Hook_OnSendPacket( &CTcpServer::OnSendPacket );
}

CTcpServer::CTcpServer()
{
	m_nListenFd = -1;
	m_strIP.clear();
	m_u16Port = 0;

	m_pCallback_IEpollEventCallback = boost::shared_ptr< CEpollEventCallbackObj< CTcpServer > >( new CEpollEventCallbackObj< CTcpServer >( this ) );
	m_pCallback_IEpollEventCallback->Hook_OnStart( &CTcpServer::OnStart );
	m_pCallback_IEpollEventCallback->Hook_OnIdle( &CTcpServer::OnIdle );
	m_pCallback_IEpollEventCallback->Hook_OnEnd( &CTcpServer::OnEnd );
	m_pCallback_IEpollEventCallback->Hook_OnEpollIn( &CTcpServer::OnEpollIn );
	m_pCallback_IEpollEventCallback->Hook_OnEpollOut( &CTcpServer::OnEpollOut );
	m_pCallback_IEpollEventCallback->Hook_OnEpollErr( &CTcpServer::OnEpollErr );
	m_pCallback_IEpollEventCallback->Hook_OnSendPacket( &CTcpServer::OnSendPacket );
}

CTcpServer::~CTcpServer ()
{
	m_pCallback_IEpollEventCallback->UnhookAll();
}

int32_t CTcpServer::Init ( boost::shared_ptr< CEpollEvent > pEpollEvent, 
		const std::string & strIP, u_int16_t u16Port,
		u_int32_t uMaxFd, boost::shared_ptr< ITcpEventCallback > pTcpEventCallback)
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

	if (m_pEpollEvent->Init (uMaxFd) != 0)
	{
		return -1;
	}

	if (m_pEpollEvent->Attach( m_pCallback_IEpollEventCallback ) != 0)
	{
		return -1;
	}

	m_strIP = strIP;
	m_u16Port = u16Port;
	m_pTcpEventCallback = pTcpEventCallback;
	return 0;
}

int32_t CTcpServer::OnStart (void *pThis)
{
	m_nListenFd = socket (AF_INET, SOCK_STREAM, 0);
	if ( -1 == m_nListenFd )
	{
		return -1;
	}

	if ( m_pTcpEventCallback )
	{
		m_pTcpEventCallback->OnNewSocket (m_nListenFd, this);
	}

	do
	{
		CNetworkUtil::SetNonBlockSocket (m_nListenFd);

		int reuse = 1;
		setsockopt (m_nListenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (int));

		/* 绑定端口 */
		if ( -1 == CNetworkUtil::Bind( m_nListenFd, m_strIP.c_str(), m_u16Port ) )
		{
			std::cout << "bind failed!!!" << "\n";
			break;
		}

		if ( -1 == listen (m_nListenFd, SOMAXCONN) )
		{
			break;
		}

		/* 注册事件, 只处理可读(连接请求) */
		if ( -1 == m_pEpollEvent->RegisterEvent( m_nListenFd, EPOLLIN, m_pCallback_IEpollEventCallback ) )
		{
			break;
		}

		if ( m_pTcpEventCallback )
		{
			m_pTcpEventCallback->OnStart( this );
		}
		return 0;
	}
	while (0);

	/* 处理失败的创建过程, 在这里, 只要关闭监听端口就可以了 */
	if (m_nListenFd >= 0)
	{
		if ( m_pTcpEventCallback )
		{
			m_pTcpEventCallback->OnError(m_nListenFd, this);
		}

		m_pEpollEvent->Close (m_nListenFd);
		m_nListenFd = -1;

		return -1;
	}

	return 0;
}

void CTcpServer::OnIdle (void *pThis)
{
}

void CTcpServer::OnEpollIn (int32_t fd, void *pThis )
{
	if ( NULL == m_pEpollEvent.get() )
	{
		return;
	}

	if (fd == m_nListenFd)
	{
		// 新连接
		sockaddr_in addr;
		socklen_t addr_len = sizeof ( addr );
		int32_t   new_fd   = accept ( fd, (sockaddr*)&addr, &addr_len);

		if ( -1 == new_fd)
		{
			return;
		}

		if (m_pTcpEventCallback)
		{
			m_pTcpEventCallback->OnNewSocket( new_fd, this);
		}

		CNetworkUtil::SetNonBlockSocket( new_fd );
		if (-1 == m_pEpollEvent->RegisterEvent(new_fd, EPOLLIN | EPOLLOUT | EPOLLET, m_pCallback_IEpollEventCallback ) )
		{
			Close (fd);
		}

		if (m_pTcpEventCallback)
		{
			if (-1 == m_pTcpEventCallback->OnConnected (new_fd, addr, this))
			{
				Close (new_fd);
			}
		}
	}
	else
	{
		// 数据到来
		if (m_pTcpEventCallback)
		{
			if ( -1 == m_pTcpEventCallback->OnDataIn(fd, this) )
			{
				Close (fd);
			}
		}
	}
}

void CTcpServer::OnEpollOut( int32_t fd, void *pThis )
{
	if (m_pTcpEventCallback.get() != NULL)
	{
		if (m_pTcpEventCallback->OnDataOut (fd, this) == -1)
		{
			Close (fd);
		}
	}
}

void CTcpServer::OnSendPacket (int32_t fd, void *pThis)
{
	if (m_pTcpEventCallback.get() != NULL)
	{
		if (m_pTcpEventCallback->OnSendPacket( fd , this ) == -1)
		{
			Close (fd);
		}
	}
}

void CTcpServer::OnEpollErr (int32_t fd, void *pThis)
{
	if (fd == m_nListenFd)
	{
		if (m_pEpollEvent)
		{
			m_pEpollEvent->Stop();
		}
	}

	if ( -1 == fd )
	{
		return;
	}

	Close (fd);
}

void CTcpServer::OnEnd (void *pThis)
{
	if ( m_nListenFd >= 0 )
	{
		if ( m_pEpollEvent )
		{
			m_pEpollEvent->Close( m_nListenFd );
		}
		m_nListenFd = -1;
	}
	return;
}

int32_t CTcpServer::Close (int32_t fd)
{
	if ( m_pTcpEventCallback )
	{
		m_pTcpEventCallback->OnClose( fd , this );
	}

	if (m_pEpollEvent)
	{
		m_pEpollEvent->Close( fd );
	}

	return 0;
}

