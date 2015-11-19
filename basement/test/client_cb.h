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

        // �յ�һ���������߼����󱻵���
        virtual int32_t OnPacketReceived( int32_t fd, sockaddr_in addr, 
                                            boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, // ���� \r\n\r\n
                                            boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                            u_int32_t uPacketId );

        // ����һ���������߼���
        virtual int32_t OnPacketSent( int32_t fd, sockaddr_in addr, 
                                        boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, 
                                        boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                        u_int32_t uPacketId );

        // ��ʱ
        virtual int32_t OnTimeout( boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, 
                                    boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
                                    u_int32_t uPacketId );

		// ����������
		virtual int32_t OnConnected(int32_t fd, sockaddr_in addr);

		// ���ӹر�ʱ��ص��ú���
		virtual int32_t OnClose(int32_t fd, sockaddr_in addr);


	protected:
		boost::shared_ptr< CMemPool >					m_pMemPool;
		boost::shared_ptr< CHttpPacketLayer >			m_pPacketLayer;
		boost::shared_ptr< CThreadPool< tagHttpEvent > >	m_pThreadPool;	
		boost::shared_ptr< CHandler >					m_pHandler;
		std::string								m_strEventSource; // �� servicemgr.h ���ж���
}; 

#endif // _CLIENT_CB_INCLUDED_
