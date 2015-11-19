#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <iostream>
#include "atomic.h"
#include "epollevent.h"

const uint32_t EPOLL_TIMEOUT = 50;


CEpollEvent::CEpollEvent( void ):
	m_epoll_fd( -1 ),
	m_ep_timeout( EPOLL_TIMEOUT ),
	m_initialized ( 0 )
{
	rlimit rlim;
	getrlimit( RLIMIT_NOFILE, &rlim );
	m_max_fd = rlim.rlim_max;

	m_bListDirty = false;
	m_bMapDirty = false;

	m_bLockExchangeCallback = false;

	InterlockedExchange32( &m_is_running, 1 );
}

CEpollEvent::~CEpollEvent( void )
{
	Uninit();
}

int32_t CEpollEvent::Init(void)
{
	rlimit rlim ;
	getrlimit(RLIMIT_NOFILE, &rlim) ;
	return Init(rlim.rlim_max) ;
}

int32_t CEpollEvent::Init(const int32_t max)
{
	if ( 0 != m_initialized )
		return 0 ;

	/* ignore SIG_PIPE */
	signal(SIGPIPE, SIG_IGN) ;

	if ( m_epoll_fd >= 0 )
	{
		::close (m_epoll_fd) ;
	}

	m_epoll_fd = epoll_create( max * 2 ) ;
	if ( -1 == m_epoll_fd)
	{
		return -1 ;
	}

	m_mapEpollEventCallback.clear();
	m_initialized = 1;
	m_max_fd = max;

	return 0 ;
}

void CEpollEvent::Uninit(void)
{
	if (0 == m_initialized)
	{
		return  ;
	}

	m_lstEpollEventCallback.clear();
	m_mapEpollEventCallback.clear();
	m_initialized = 0 ;
	return ;
}

void CEpollEvent::SetTimeout(const int32_t timeout)
{
	m_ep_timeout = timeout ;
}

int32_t CEpollEvent::Attach( boost::shared_ptr< IEpollEventCallback > pEpollEventCallback )
{
	if (pEpollEventCallback.get() == NULL)
		return -1 ;

	for( CLstEpollEvent::iterator itr = m_lstEpollEventCallback.begin(); itr != m_lstEpollEventCallback.end(); itr++ )
	{
		if ( ( *itr ) == pEpollEventCallback )
		{
			return -1;
		}
	}

	m_lstEpollEventCallback.push_back( pEpollEventCallback );
	m_lstJustAdded.push_back( pEpollEventCallback );

	return 0;
}

int32_t CEpollEvent::Detach( boost::shared_ptr< IEpollEventCallback > pEpollEventCallback )
{
	for( CLstEpollEvent::iterator itr = m_lstEpollEventCallback.begin(); itr != m_lstEpollEventCallback.end(); itr++ )
	{
		if( *itr == pEpollEventCallback )
		{
			m_lstEpollEventCallback.erase( itr );
			m_bListDirty = true;
			return 0;
		}
	}

	return -1;
}

void CEpollEvent::OnEpollWaitError(void)
{
	CMapEpollEventCallback mapEpollEventCallback; 

	if( m_bMapDirty )
	{
		mapEpollEventCallback = m_mapEpollEventCallback;
	}
	else
	{
		mapEpollEventCallback = m_mapEpollEventCallback;
	}

	for( CMapEpollEventCallback::iterator itr = mapEpollEventCallback.begin();
			itr != mapEpollEventCallback.end(); itr++ )
	{
		if ( itr->second != NULL )
		{
			itr->second->OnEpollErr( itr->first, this );
		}
	}

	m_mapEpollEventCallback.clear();
	m_bMapDirty = true;
	return ;
}

int32_t CEpollEvent::Run(void)
{
	if ( m_epoll_fd == -1)
	{
		return -1;
	}

	int32_t wait_ret = 0 ;
	int32_t curr_fd = -1 ;
	u_int32_t curr_event = 0 ;

	CMapEpollEventCallback::iterator    itrCallback;

	if( m_lstJustAdded.empty() == false )
	{
		for( CLstEpollEvent::iterator itr = m_lstJustAdded.begin(); itr != m_lstJustAdded.end(); itr++ )
		{
			if( *itr != NULL )
			{
				if( ( *itr )->OnStart( this ) != 0 )
				{
					return -1;
				}
			}
		}
		m_lstJustAdded.clear();
	}

	m_bLockExchangeCallback = true;

	InterlockedExchange32(&m_is_running, 1) ; /* not garuanteed */
	while(InterlockedExchange32(&m_is_running, 1) == 1 /* stop check */)
	{
		// 给外部一次修改的机会，外部在这里可以建立新连接，关闭现有连接
		for( CSetEpollEventExchange::iterator itr = m_setEpollEventExchangeCallback.begin(); itr != m_setEpollEventExchangeCallback.end(); itr++ )
		{
			if( *itr )
			{
				( *itr )->OnExchange( this );
			}
		}

		if( m_lstJustAdded.empty() == false )
		{
			for( CLstEpollEvent::iterator itr = m_lstJustAdded.begin(); itr != m_lstJustAdded.end(); itr++ )
			{
				if( *itr != NULL )
				{
					if( ( *itr )->OnStart( this ) != 0 )
					{
						return -1;
					}
				}
			}
			m_lstJustAdded.clear();
		}

		/* 最先调用on_idle主要是考虑到有些扩展类可能会在这里第一次更新时间 */
		for( CLstEpollEvent::iterator itr = m_lstEpollEventCallback.begin(); itr != m_lstEpollEventCallback.end(); itr++ )
		{
			if( *itr )
			{
				( *itr )->OnIdle( this );
			}
		}

		// 真正的发包在这里和下面的 epoll_out 事件 
		for( CMapEpollEventCallback::iterator itr = m_mapEpollEventCallback.begin(); itr != m_mapEpollEventCallback.end(); itr++ )
		{
			if( itr->second != NULL )
			{
				itr->second->OnSendPacket( itr->first, this );
			}
		}

		wait_ret = epoll_wait( m_epoll_fd, m_events, sizeof(m_events)/sizeof(m_events[0]), m_ep_timeout );

		if (wait_ret == -1)
		{
			/* error procedure, 
			 * clear all the fds and notify the outter server
			 */
			int32_t e = errno ;
			if (e == EINTR)
				continue ;

			OnEpollWaitError() ;
			break ;
		}

		for (int32_t i = 0 ; i < wait_ret ; ++i)
		{
			curr_fd = m_events[i].data.fd ;
			curr_event = m_events[i].events ;

			itrCallback = m_mapEpollEventCallback.find( curr_fd );
			boost::shared_ptr< IEpollEventCallback > pEpollEventCallback = itrCallback->second; // retain the pointer in case it is freed by those callback.
			if( itrCallback == m_mapEpollEventCallback.end() )
			{
				continue;
			}

			if( pEpollEventCallback == NULL )
			{
				continue;
			}

			if (curr_event & (EPOLLERR | EPOLLHUP))
			{
				pEpollEventCallback->OnEpollErr( curr_fd, this );
			} 

			if (curr_event & EPOLLIN)
			{
				pEpollEventCallback->OnEpollIn( curr_fd, this );
			}

			if (curr_event & EPOLLOUT)
			{
				pEpollEventCallback->OnEpollOut( curr_fd, this ); // itrCallback->second might be freed here, so I retain it as pEpollEventCallback.
				pEpollEventCallback->OnSendPacket( curr_fd, this );
			}

		}
	}
	InterlockedExchange32(&m_is_running, 0);

	for( CLstEpollEvent::iterator itr = m_lstEpollEventCallback.begin(); itr != m_lstEpollEventCallback.end(); itr++ )
	{
		( *itr )->OnEnd( this );
	}

	return 0 ;

}

int32_t CEpollEvent::Stop(void)
{
	return InterlockedExchange32(&m_is_running, 0) ;
}

int32_t CEpollEvent::RegisterEvent( const int32_t fd, const uint32_t event, boost::shared_ptr< IEpollEventCallback > pEpollEventCallback )
{
	if ( ( -1 == m_epoll_fd) || ( fd < 0) || ( NULL == pEpollEventCallback.get() )  )
	{
		return -1;
	}

	if( m_mapEpollEventCallback.size() >= m_max_fd )
	{
		return -1;
	}

	int32_t ret_val = CNetworkUtil::RegisterEvent( m_epoll_fd, fd, event);
	if (ret_val == 0) // success 
	{
		m_mapEpollEventCallback[ fd ] = pEpollEventCallback;
		m_bMapDirty = true;
	}

	return ret_val;
}

int32_t CEpollEvent::UnregisterEvent(const int32_t fd)
{
	if (fd < 0)
	{
		return -1;
	}

	CNetworkUtil::UnregisterEvent( m_epoll_fd, fd);
	m_mapEpollEventCallback.erase( fd );
	m_bMapDirty = true;
	return 0 ;
}

int32_t CEpollEvent::Close(int32_t fd) 
{
	if (fd > 0)
	{
		m_mapEpollEventCallback.erase( fd );
		m_bMapDirty = true;
	}
	return ::close(fd) ;
}

int32_t	CEpollEvent::AddExchangeCallback( boost::shared_ptr< IEpollEventExchange > pEpollEventExchangeCallback )
{
	if( m_bLockExchangeCallback )
	{
		return -1;
	}

	m_setEpollEventExchangeCallback.insert( pEpollEventExchangeCallback );

	return 0;
}

int32_t	CEpollEvent::DelExchangeCallback( boost::shared_ptr< IEpollEventExchange > pEpollEventExchangeCallback )
{
	if( m_bLockExchangeCallback )
	{
		return -1;
	}

	m_setEpollEventExchangeCallback.erase( pEpollEventExchangeCallback );

	return 0;
}

