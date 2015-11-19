// author:chenjie

#ifndef _SERVER_THREAD_INCLUDED_
#define _SERVER_THREAD_INCLUDED_

#include <map>
#include <string>
#include <boost/smart_ptr.hpp>
#include "mem_pool.h"
#include "epollevent.h"
#include "timeoutmgr.h"
#include "tcpserver.h"
#include "tcppacket.h"
#include "thread_model.h"

class CServerThread : public CThreadModel
{
	public:
		CServerThread();
		virtual ~CServerThread();

	public:
		virtual	int32_t	Create( boost::shared_ptr< CPacketLayer > pPacketLayer, const std::string &strIP, u_int16_t u16Port,
				bool bSingleThread = false ); 
		virtual	int32_t Run( void *pArg );
		virtual int32_t Stop();
		virtual	int32_t Join();
		virtual	int32_t	IsRunning( bool *pbRunning );

		// 作为 server，通常是发送答包，所以 SendPacket 函数不同于 client 指定 ip:port，这里直接指定 fd
		virtual	int32_t	SendPacket( int32_t fd, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId );

	protected:			
		virtual	int32_t	RunOnce();

	protected:
		boost::shared_ptr< CEpollEvent >	m_pEpollEvent;
		boost::shared_ptr< CTcpServer >	m_pTcpServer;
		boost::shared_ptr< CPacketLayer >	m_pPacketLayer;

		bool						m_bSingleThread; // 当外部传入已经创建的 epoll ，这时候会使用单线程模式，设置这个变量为 true
};

#endif // _SERVER_THREAD_INCLUDED_
