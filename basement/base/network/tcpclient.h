// 异步的TCPClient，可以用作简单的连接池
//  一个tcpclient只连接一个 ip:port，但可对这个ip:port建立多个连接
// author:chenjie

#ifndef _TCP_CLIENT_INCLUDED_
#define _TCP_CLIENT_INCLUDED_

#include "epollevent.h"
#include "tcppeer.h"
#include "object_pool.h"
#include "cppsink.h"
#include <string>
#include <vector>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/smart_ptr.hpp>

enum EConnectionStatus
{
	CONN_STATUS_NOT_CONNECTED = -1,
	CONN_STATUS_CONNECTED = 1,
};

typedef std::map< u_int32_t, EConnectionStatus >	CMapConnContext; // fd--conn status

class CTcpClient
{
	public:
		CTcpClient();
		CTcpClient( boost::shared_ptr< CEpollEvent > pEpollEvent );
		virtual ~CTcpClient();

	public:
		// ITcpEventCallback
		virtual int32_t     OnStart( void *pThis ); 
		virtual void        OnIdle (void *pThis);
		virtual void        OnEnd(void *pThis);
		virtual void        OnEpollIn   ( int32_t fd, void *pThis );
		virtual void        OnEpollOut  ( int32_t fd, void *pThis );
		virtual void        OnEpollErr  ( int32_t fd, void *pThis );
		virtual void        OnSendPacket( int32_t fd, void *pThis );

	public:
		virtual	int32_t     Init( boost::shared_ptr< CEpollEvent > pEpollEvent, u_int32_t uMaxFd, boost::shared_ptr< ITcpEventCallback > pTcpEventCallback, 
				const std::string &strIP, uint16_t u16Port, bool bAutoReconnect, time_t tmReconnectTimeout, int32_t nConnCount ); 
		virtual uint16_t    GetServerPort();
		virtual std::string GetServerIP();

	public:
		virtual	int32_t     Close( int32_t fd ); // close 函数之后，有可能这个 tcpclient 已经被释放了，所以不能在调用 Close 之后再调用任何成员函数

		virtual	int32_t     Connect( const std::string &strIP, uint16_t u16Port, int32_t *pfd ); 

		virtual	int32_t     GetConnectedFd( std::vector< int32_t > *pVecFd );

	protected:
		virtual	time_t	    GetTimeStamp(); // 单位毫秒
		virtual	int32_t	    Reconnect(); // 断线重连

	protected:
		boost::shared_ptr< CEpollEvent >	m_pEpollEvent;
		CMapConnContext             		m_mapConnContext;

		bool                        		m_bAutoReconnect;           // 是否自动重连
		time_t                   		   	m_tmReconnectTimeout;       // 重连超时
		time_t                   		    m_tmLastTimeReconnect;      // 上次重连的时间
		int32_t                  		    m_nConnCount;               // 期望建立多少个连接
		int32_t                  		    m_nCurrentConnCount;        // 当前建立了多少个连接

		std::string                	 		m_strIP;
		uint16_t                    		m_u16Port;

		boost::shared_ptr< ITcpEventCallback >	m_pTcpEventCallback;

		boost::recursive_mutex      		m_lockClient;

		boost::shared_ptr< CEpollEventCallbackObj< CTcpClient > >	m_pCallback_IEpollEventCallback;
};

extern TObjectPool< CTcpClient >	g_objPool_TcpClient;


#endif // _TCP_CLIENT_INCLUDED_
