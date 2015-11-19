#ifndef _CLIENT_CB_INCLUDED_
#define _CLIENT_CB_INCLUDED_

#include <sys/types.h>
#include <boost/smart_ptr.hpp>

#include "epollevent.h"
#include "timeoutmgr.h"
#include "httppacket.h"

#include "handler.h" 

class CClientPacketCb : public IHttpPacketCallback 
{
	public:
		CClientPacketCb();
		virtual ~CClientPacketCb();

	public:
		virtual	int32_t	Init( const std::string &strEventSource,
				boost::shared_ptr< CMemPool > pMemPool,
				boost::shared_ptr< CHttpPacketLayer > pPacketLayer,
				boost::shared_ptr< CThreadPool< tagHttpEvent > > pThreadPool,
				boost::shared_ptr< CHandler > pHandler );

        // 收到一个完整的逻辑包后被调用
        virtual int32_t OnPacketReceived( int32_t fd, sockaddr_in addr, 
                                            boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, // 包括 \r\n\r\n
                                            boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                            u_int32_t uPacketId );

        // 发出一个完整的逻辑包
        virtual int32_t OnPacketSent( int32_t fd, sockaddr_in addr, 
                                        boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, 
                                        boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                        u_int32_t uPacketId );

        // 超时
        virtual int32_t OnTimeout( boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, 
                                    boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                    u_int32_t uPacketId );

		// 有连接连入
		virtual int32_t OnConnected(int32_t fd, sockaddr_in addr);

		// 连接关闭时会回调该函数
		virtual int32_t OnClose(int32_t fd, sockaddr_in addr);


	protected:
		boost::shared_ptr< CMemPool >					m_pMemPool;
		boost::shared_ptr< CHttpPacketLayer >			m_pPacketLayer;
		boost::shared_ptr< CThreadPool< tagHttpEvent > >	m_pThreadPool;	
		boost::shared_ptr< CHandler >					m_pHandler;
		std::string								m_strEventSource; // 在 servicemgr.h 中有定义
}; 

#endif // _CLIENT_CB_INCLUDED_
