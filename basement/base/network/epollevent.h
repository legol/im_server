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
class IEpollEventCallback // �� tcp/udp ���������������¼�
{
	public:
		IEpollEventCallback() {}
		virtual ~IEpollEventCallback() {}

		virtual int32_t OnStart     ( void *pThis ) { return 0; }    //��������������˿�
		virtual void    OnIdle      ( void *pThis ) {}               //����������ִ������
		virtual void    OnEnd       ( void *pThis ) {}
		virtual void    OnEpollIn   ( int32_t fd, void *pThis ) {}
		virtual void    OnEpollOut  ( int32_t fd, void *pThis ) {}
		virtual void    OnEpollErr  ( int32_t fd, void *pThis ) {}
		virtual void    OnSendPacket( int32_t fd, void *pThis ) {} // ��������� send( fd, xxxx )
};
typedef std::list< boost::shared_ptr< IEpollEventCallback > >			CLstEpollEvent;
typedef std::map< int32_t, boost::shared_ptr< IEpollEventCallback > > 	CMapEpollEventCallback;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IEpollEventExchange // ���ⲿ���κεط����������޸� epollevent ���ڲ����ݽṹ�Ͳ��������͵ģ�������ص�����������/�Ͽ�����
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

		// ����ص�ֻ�����߳����������޸ģ������� epoll �߳�û������ʱ���޸ģ�һ�� epoll �߳��������Ͳ������޸ģ���Ϊ epollevent �����κ��߳�ͬ������
		int32_t	AddExchangeCallback( boost::shared_ptr< IEpollEventExchange > pEpollEventExchangeCallback );
		int32_t	DelExchangeCallback( boost::shared_ptr< IEpollEventExchange > pEpollEventExchangeCallback );

		int32_t Init  ( void );
		int32_t Init  ( const int32_t max );
		void    Uninit( void );
		int32_t Run   ( void );
		int32_t Stop  ( void );
		int32_t Close ( const int32_t fd);
		void    SetTimeout   (const int32_t timeout);

		// ע��Epoll�¼����ڲ����¼�ʱ���ص�
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
		uint32_t    m_max_fd;              //���ͬʱ�����fd����
		int32_t     m_initialized;
		uint32_t volatile m_is_running;

		// fd--�ص���ӳ�䣬ĳ��fd�����¼�����Ҫ�����ĸ��ص��ӿ�
		CLstEpollEvent          m_lstEpollEventCallback; // ���и��������Ļص��ӣ���run֮ǰattach�����ģ����ǵ�onstart�ᱻ����
		CMapEpollEventCallback  m_mapEpollEventCallback; // fd--�ص���ӳ�䣬ĳ��fd�����¼�����Ҫ�����ĸ��ص��ӿ�

		CLstEpollEvent			m_lstJustAdded; // �ձ����뵽�ص��� tcp client���������ķ��� OnStart �Ļص���Ȼ�����

		bool					m_bListDirty;
		bool					m_bMapDirty;

		// ����ص�ֻ�����߳����������޸ģ������� epoll �߳�û������ʱ���޸ģ�һ�� epoll �߳��������Ͳ������޸ģ���Ϊ epollevent �����κ��߳�ͬ������
		bool					m_bLockExchangeCallback;
		CSetEpollEventExchange	m_setEpollEventExchangeCallback;
};

#endif  //  _EPOLLEVENT_INCLUDED_
