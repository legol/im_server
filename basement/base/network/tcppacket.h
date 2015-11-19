// Packet layer of TCP-Async Server
// ���ڴ���������ͷ+�䳤���塱�����İ��ṹ
// author: chenjie

#ifndef _TCP_PACKET_INCLUDED_
#define _TCP_PACKET_INCLUDED_

#include <sys/types.h>
#include <boost/smart_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <map>
#include <list>
#include "tcpserver.h"
#include "timeoutmgr.h"
#include "mem_pool.h"


// ����Ψһ�� packet_id������Ψһ�ı�ʶһ���߼���
// packet_id �϶������� 0

#ifndef NEED_RESPONSE
const bool NEED_RESPONSE    = true;
#endif
#ifndef NEED_NO_RESPONSE
const bool NEED_NO_RESPONSE = false;
#endif


/// forward declaration
class CPacketLayer;

// ��������Ͳ��
// ��һ���߼����ֲ��η��ͳ�ȥ�����ͳ�ȥһ���������߼������ټ������Ͷ����е���һ���߼���
// ���յ����������һ�������߼������ϲ㷢���ص�

/////////////////////////////////////////////////////////////////////////////////////////////
class IPacketCallback : public ITimeoutCallback
{
public:
	IPacketCallback(){};
	virtual ~IPacketCallback(){};

public:
	// ask caller for the expected header length
	virtual	int32_t OnGetPacketHeaderLen( int32_t fd, sockaddr_in addr, u_int32_t *puHeaderLen ){ *puHeaderLen = sizeof(int32_t); return 0; }; 

	// ask caller for the expected body length
	virtual	int32_t OnGetPacketBodyLen( int32_t fd, sockaddr_in addr, boost::shared_array< u_int8_t > pchHeader,
			u_int32_t uHeaderLen, u_int32_t *puBodyLen ){ return -1; }; 

	// 1. packet_id is used to match request and response.
	// 2. packet_id is a unique number which can be used by timeout control. When a packet is received, we'll check whether the packet is still watched by timeout manager.
	// If the timeout manager doesn't find it, the packet was timeout.
	virtual	int32_t	OnGetPacketId(int32_t fd, sockaddr_in addr, 
			boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, bool bIsOutgoingPacket, u_int32_t *puPacketId ){ return -1; }; 	

	// a full packet is received.
	virtual	int32_t	OnPacketReceived( int32_t fd, sockaddr_in addr, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ){ return -1; }; 

	// You can determin where to put the packet_id. And now it is the chance you can put it into your packet.
	virtual	int32_t	OnPreSendPacket( int32_t fd, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId, void* pvParam = NULL){ return -1; };

	// a full packet was sent.
	virtual	int32_t	OnPacketSent( int32_t fd, sockaddr_in addr, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ){ return -1; }; 

	// some request timeout. we hasn't received any response for the packet identified by uPacketId.
	virtual	int32_t OnTimeout( boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ){ return -1; };

	// ITcpEventCallback�
	virtual int32_t	OnConnected(int32_t fd, sockaddr_in addr) { return 0; };
	virtual int32_t	OnConnectionRefused(int32_t fd, sockaddr_in addr) { return -1; };
	virtual int32_t	OnClose(int32_t fd, sockaddr_in addr) { return -1; };
	virtual int32_t	OnError(int32_t fd, sockaddr_in addr) { return -1; };
	virtual	void	OnStart( void ) {};

	/// < To break cyclic dependency with CPacketLayer
	/// use weak_ptr to hold the reference to CPacketLayer >
	virtual void SetPacketLayer( boost::shared_ptr< CPacketLayer > pPacketLayer ) { return; };
};


/////////////////////////////////////////////////////////////////////////////////////////////
struct tagIncomingPacket
{
	boost::shared_array< u_int8_t > pPacketHead;
	u_int32_t                       uPacketHeadLen, uPacketHeadReceived;

	u_int8_t						*pPacketBody; // ָ�� pPacket �� body ����ʼλ��
	u_int32_t                       uPacketBodyLen, uPacketBodyReceived;

	boost::shared_array< u_int8_t > pPacket; // ��ͷ�Ӱ���

	tagIncomingPacket()
	{
		uPacketHeadLen = 0;
		uPacketHeadReceived = 0;

		uPacketBodyLen = 0;
		uPacketBodyReceived = 0;

		pPacketBody = NULL;
	}

	const tagIncomingPacket& operator=( const tagIncomingPacket& right )
	{
		pPacketHead = right.pPacketHead;
		uPacketHeadLen = right.uPacketHeadLen;
		uPacketHeadReceived = right.uPacketHeadReceived;

		pPacketBody = right.pPacketBody;
		uPacketBodyLen = right.uPacketBodyLen;
		uPacketBodyReceived = right.uPacketBodyReceived;

		pPacket = right.pPacket;

		return *this;
	}
};
typedef std::map< int32_t, tagIncomingPacket >  CMapReceiving;

/////////////////////////////////////////////////////////////////////////////////////////////
struct tagOutgoingPacket
{
	int32_t                         fd;
	boost::shared_array< u_int8_t > pPacket;
	u_int32_t                       uPacketLen, uPacketSent;
	bool                            bNeedResp;

	tagOutgoingPacket()
	{
		uPacketLen = 0;
		uPacketSent = 0;
		bNeedResp = false;
	}
};
typedef std::list< tagOutgoingPacket >    CLstSending;
typedef std::map< int32_t, CLstSending >  CMapSending; // fd--��δ���͵�packet


class CPacketId;

/////////////////////////////////////////////////////////////////////////////////////////////
// ��������̰߳�ȫ�ģ�Ҳ�����������Ψһ�ӿ�
class CPacketLayer 
{
	public:
		CPacketLayer();
		virtual ~CPacketLayer();

	public:
		virtual	int32_t	Init( boost::shared_ptr< IPacketCallback > pPacketCb, 
				boost::shared_ptr< CMemPool > pMemPool, 
				boost::shared_ptr< CPacketId > pPacketId, 
				boost::shared_ptr< CTimeoutMgr > pTimeoutMgr, u_int32_t uTimeoutInterval );

		// ��Ҫ���͵İ��ŵ����оͷ���,epoll ���̻߳���epollout��ʱ��ƴ�������ⷢ���������пջ���EAGAIN
		virtual	int32_t	SendPacket( int32_t fd, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, bool bNeedResp, u_int32_t *puPacketId, void* pvParam = NULL); 

	public:

		// ITcpEventCallback
		virtual int32_t	OnConnected(int32_t fd, sockaddr_in addr, void *pThis);
		virtual int32_t	OnConnectionRefused(int32_t fd, void *pThis);
		virtual int32_t	OnDataIn    ( int32_t fd, void *pThis );
		virtual int32_t	OnDataOut   ( int32_t fd, void *pThis );
		virtual int32_t	OnNewSocket ( int32_t fd, void *pThis );
		virtual int32_t	OnClose     ( int32_t fd, void *pThis );
		virtual int32_t	OnError     ( int32_t fd, void *pThis );
		virtual int32_t	OnSendPacket( int32_t fd, void *pThis );
		virtual	void	OnStart		( void *pThis );


		virtual	boost::shared_ptr< ITcpEventCallback >	GetTcpEventCallback();
	protected:
		CMapReceiving			m_mapReceiving; // ���ڽ��գ���δ����İ�,��� m_mapReceiving[ fd ] �����ݣ���ô��˵�����fd����δ����İ�
		CMapSending				m_mapSending; // ���ڷ��ͺͻ�δ���͵İ�����ͬ m_mapReceiving ������΢��ͬ����m_mapSending[ fd ] ��Ҫ������ fd �����а�
		CMapSocketStatus        m_mapSocketStatus; // �ܶ�Ӧ epoll ��ص� fd ����Ϣ	

		boost::shared_ptr< CMemPool >		m_pMemPool;
		boost::shared_ptr< CPacketId >		m_pPacketId;
		boost::shared_ptr< CTimeoutMgr >	m_pTimeoutMgr;

		boost::shared_ptr< IPacketCallback >	m_pPacketCallback;

		boost::recursive_mutex  m_lockPacketLayer;		//��mapReceiving��SocketStatusͬʱ����
		boost::recursive_mutex  m_lockSend;				//��mapSending���м���������recv��ͬʱsend

		u_int32_t				m_uTimeoutInterval;

		boost::shared_ptr< CTcpEventCallbackObj< CPacketLayer >	> 		m_pCallback_ITcpEventCallback;

	public:
		bool					m_bIsClient; // �Ƿ���Ϊ client ����
};


#endif // _TCP_PACKET_INCLUDED_
