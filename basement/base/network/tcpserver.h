#ifndef _TCP_SERVER_INCLUDED_
#define _TCP_SERVER_INCLUDED_

#include "epollevent.h"
#include "tcppeer.h"
#include <boost/smart_ptr.hpp>

class CTcpServer
{
	public:
		CTcpServer();
		CTcpServer ( boost::shared_ptr< CEpollEvent > pEpollEvent );
		virtual ~CTcpServer ();

		// IEpollEventCallback
		virtual int32_t OnStart      ( void *pThis );
		virtual void    OnIdle       ( void *pThis );
		virtual void    OnEnd        ( void *pThis );
		virtual void    OnEpollIn    ( int32_t fd, void *pThis );
		virtual void    OnEpollOut   ( int32_t fd, void *pThis );
		virtual void    OnEpollErr   ( int32_t fd, void *pThis );
		virtual void    OnSendPacket ( int32_t fd, void *pThis );

		int32_t Init ( boost::shared_ptr< CEpollEvent > pEpollEvent,
				const std::string &strIP, uint16_t u16Port,
				uint32_t uMaxFd, boost::shared_ptr< ITcpEventCallback > pTcpEventCallback);

	protected:
		virtual int32_t Close (int32_t fd);

		boost::shared_ptr< CEpollEvent>			m_pEpollEvent;
		int32_t         m_nListenFd;
		std::string     m_strIP;
		uint16_t        m_u16Port;

		boost::shared_ptr< ITcpEventCallback > 	m_pTcpEventCallback;

		boost::shared_ptr< CEpollEventCallbackObj< CTcpServer > >	m_pCallback_IEpollEventCallback;
};

#endif // _TCP_SERVER_INCLUDED_
