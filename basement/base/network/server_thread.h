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

		// ��Ϊ server��ͨ���Ƿ��ʹ�������� SendPacket ������ͬ�� client ָ�� ip:port������ֱ��ָ�� fd
		virtual	int32_t	SendPacket( int32_t fd, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId );

	protected:			
		virtual	int32_t	RunOnce();

	protected:
		boost::shared_ptr< CEpollEvent >	m_pEpollEvent;
		boost::shared_ptr< CTcpServer >	m_pTcpServer;
		boost::shared_ptr< CPacketLayer >	m_pPacketLayer;

		bool						m_bSingleThread; // ���ⲿ�����Ѿ������� epoll ����ʱ���ʹ�õ��߳�ģʽ�������������Ϊ true
};

#endif // _SERVER_THREAD_INCLUDED_
