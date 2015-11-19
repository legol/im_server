#ifndef _EPOLLEVENT_INCLUDED_
#define _EPOLLEVENT_INCLUDED_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <cstring>
#include <map>
#include <set>
#include <boost/smart_ptr.hpp>
#include "networkutil.h"
#include "cppsink.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IEpollEventCallback // 给 tcp/udp 层用来处理网络事件
{
	public:
		IEpollEventCallback() {}
		virtual ~IEpollEventCallback() {}

		virtual int32_t OnStart     ( void *pThis ) { return 0; }    //可以在这里监听端口
		virtual void    OnIdle      ( void *pThis ) {}               //可以在这里执行重连
		virtual void    OnEnd       ( void *pThis ) {}
		virtual void    OnEpollIn   ( int32_t fd, void *pThis ) {}
		virtual void    OnEpollOut  ( int32_t fd, void *pThis ) {}
		virtual void    OnEpollErr  ( int32_t fd, void *pThis ) {}
		virtual void    OnSendPacket( int32_t fd, void *pThis ) {} // 在这里调用 send( fd, xxxx )
};
typedef std::list< boost::shared_ptr< IEpollEventCallback > >			CLstEpollEvent;
typedef std::map< int32_t, boost::shared_ptr< IEpollEventCallback > > 	CMapEpollEventCallback;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IEpollEventExchange // 给外部（任何地方）机会来修改 epollevent 的内部数据结构和参数，典型的，在这个回调发起新连接/断开连接
{
	public:
		IEpollEventExchange(){}
		virtual ~IEpollEventExchange(){}

		virtual	int32_t	OnExchange( void *pThis ) { return -1; }
};
typedef std::set< boost::shared_ptr< IEpollEventExchange > >			CSetEpollEventExchange;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPP_SINK_BEGIN( CEpollEventCallback ) 
	CPP_SINK_FUNC( int32_t, OnStart, ( void *pThis ), ( pThis ) )
	CPP_SINK_FUNC( void, OnIdle, ( void *pThis ), ( pThis ) )
	CPP_SINK_FUNC( void, OnEnd, ( void *pThis ), ( pThis ) )
	CPP_SINK_FUNC( void, OnEpollIn, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( void, OnEpollOut, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( void, OnEpollErr, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( void, OnSendPacket, ( int32_t fd, void *pThis ), ( fd, pThis ) )
CPP_SINK_END()
DECLARE_CPP_SINK_OBJ( CEpollEventCallback, IEpollEventCallback )

CPP_SINK_BEGIN( CEpollEventExchange ) 
	CPP_SINK_FUNC( int32_t, OnExchange, ( void *pThis ), ( pThis ) )
CPP_SINK_END()
DECLARE_CPP_SINK_OBJ( CEpollEventExchange, IEpollEventExchange )



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CEpollEvent
{
	public:
		CEpollEvent( void );
		virtual ~CEpollEvent( void);

		int32_t Attach( boost::shared_ptr< IEpollEventCallback > pEpollEventCallback );
		int32_t Detach( boost::shared_ptr< IEpollEventCallback > pEpollEventCallback );

		// 这个回调只在主线程里面允许修改，并且在 epoll 线程没启动的时候修改，一旦 epoll 线程启动，就不允许修改，因为 epollevent 不带任何线程同步机制
		int32_t	AddExchangeCallback( boost::shared_ptr< IEpollEventExchange > pEpollEventExchangeCallback );
		int32_t	DelExchangeCallback( boost::shared_ptr< IEpollEventExchange > pEpollEventExchangeCallback );

		int32_t Init  ( void );
		int32_t Init  ( const int32_t max );
		void    Uninit( void );
		int32_t Run   ( void );
		int32_t Stop  ( void );
		int32_t Close ( const int32_t fd);
		void    SetTimeout   (const int32_t timeout);

		// 注册Epoll事件，在产生事件时将回调
		int32_t RegisterEvent  ( const int32_t fd, const uint32_t event, boost::shared_ptr< IEpollEventCallback > pEpollEventCallback );
		int32_t UnregisterEvent( const int32_t fd );
	protected:
		void    OnEpollWaitError( void );

	private:
		enum
		{
			MAX_EVENTS = 1024,
		};
		int32_t     m_epoll_fd;
		::epoll_event m_events[MAX_EVENTS];
		int32_t     m_ep_timeout;
		uint32_t    m_max_fd;              //最多同时处理的fd个数
		int32_t     m_initialized;
		uint32_t volatile m_is_running;

		// fd--回调的映射，某个fd产生事件后需要发给哪个回调接口
		CLstEpollEvent          m_lstEpollEventCallback; // 所有附着上来的回调子，在run之前attach上来的，他们的onstart会被调用
		CMapEpollEventCallback  m_mapEpollEventCallback; // fd--回调的映射，某个fd产生事件后需要发给哪个回调接口

		CLstEpollEvent			m_lstJustAdded; // 刚被加入到回调的 tcp client，会对这里的发出 OnStart 的回调，然后清空

		bool					m_bListDirty;
		bool					m_bMapDirty;

		// 这个回调只在主线程里面允许修改，并且在 epoll 线程没启动的时候修改，一旦 epoll 线程启动，就不允许修改，因为 epollevent 不带任何线程同步机制
		bool					m_bLockExchangeCallback;
		CSetEpollEventExchange	m_setEpollEventExchangeCallback;
};

#endif  //  _EPOLLEVENT_INCLUDED_
