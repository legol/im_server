// author:chenjie

#ifndef _CLIENT_THREAD_INCLUDED_
#define _CLIENT_THREAD_INCLUDED_

#include <map>
#include <string>
#include <boost/smart_ptr.hpp>
#include "mem_pool.h"
#include "epollevent.h"
#include "timeoutmgr.h"
#include "tcpclient.h"
#include "tcppacket.h"
#include "thread_model.h"

typedef std::map< std::string, boost::shared_ptr< CTcpClient > >	CMapClient;

struct tagClientInfo
{
	std::string		strIP;
	u_int16_t		u16Port;
	bool			bAutoReconnect;
	u_int32_t		uConnCount;
	u_int32_t		uTimeoutInterval; // 超时时间，单位：ms

	tagClientInfo()
	{
		u16Port = 0;
		bAutoReconnect = true;
		uConnCount = 5;
		uTimeoutInterval = 200;
	}
};

typedef std::vector< tagClientInfo >	CVecClientInfo;

class CClientThread : public CThreadModel
{
	public:
		CClientThread();
		virtual ~CClientThread();

	public:

		virtual	int32_t	Create( boost::shared_ptr< CPacketLayer > pPacketLayer, const CVecClientInfo &vecClientInfo, bool bSingleThread = false );
		virtual	int32_t Run( void *pArg );
		virtual int32_t Stop();
		virtual	int32_t Join();
		virtual	int32_t	IsRunning( bool *pbRunning );
		virtual	int32_t	SendPacket( const std::string &strIP, u_int16_t u16Port, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen,
				bool bNeedResp, u_int32_t *puPacketId, void *pvParam=NULL );

	protected:			
		virtual	int32_t	RunOnce();

	protected:
		virtual int32_t	GetTcpClient( const std::string &strIP, u_int16_t u16Port, boost::shared_ptr< CTcpClient > *ppTcpClient );

	protected:
		boost::shared_ptr< CEpollEvent >	m_pEpollEvent;
		CMapClient							m_mapClient;
		boost::shared_ptr< CPacketLayer >	m_pPacketLayer; // 保存这个变量用于发包
		bool								m_bSingleThread; // 当外部传入已经创建的 epoll ，这时候会使用单线程模式，设置这个变量为 true
};


#endif // _CLIENT_THREAD_INCLUDED_
