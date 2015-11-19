#ifndef _HTTP_PACKET_INCLUDED_
#define _HTTP_PACKET_INCLUDED_

// http �����շ�ʱ������߼�
// author: chenjie

#include <sys/types.h>
#include <boost/smart_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <map>
#include <list>
#include "tcpclient.h"
#include "timeoutmgr.h"
#include "mem_pool.h"
#include "tcppeer.h"
#include "cppsink.h"


/////////////////////////////////////////////////////////////////////////////////////////////
class IHttpPacketCallback : public ITimeoutCallback
{
	public:
		IHttpPacketCallback(){};
		virtual ~IHttpPacketCallback(){};

	public:
		// �յ�һ���������߼����󱻵���
		virtual	int32_t	OnPacketReceived( int32_t fd, sockaddr_in addr, 
				boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, // ���� \r\n\r\n
				boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
				u_int32_t uPacketId ){ return -1; }; 

		// ��������һ����֮ǰ�����ã�������ص�������Եõ��ײ����� packet_id �������ڽ����յ������ʱ�����֪�����ĸ�������Ĵ��
		virtual	int32_t	OnPreSendPacket( const std::string &strURL, 
				boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId, void* pvParam = NULL){ return 0; }; // todo return -1

		// ����һ���������߼���
		virtual	int32_t	OnPacketSent( int32_t fd, sockaddr_in addr, 
				boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen,		
				u_int32_t uPacketId ){ return 0; }; // todo return -1 

		// ��ʱ
		virtual	int32_t OnTimeout( boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ){ return -1; };

		// �� ITcpEventCallback ͸��������
		virtual int32_t	OnConnected(int32_t fd, sockaddr_in addr) { return -1; };
		virtual int32_t	OnConnectionRefused(int32_t fd, sockaddr_in addr) { return -1; };
		virtual int32_t	OnClose(int32_t fd, sockaddr_in addr) { return -1; };
		virtual int32_t	OnError(int32_t fd, sockaddr_in addr) { return -1; };
		virtual	void	OnStart( void ) {};
};


/////////////////////////////////////////////////////////////////////////////////////////////
// http �հ�
// 128K �ĳ�ʼ���ջ�������С,ÿ�λ����С������ʱ��,���·��䵱ǰ��С 2 ���Ļ�����
#define DEF_RECEIVING_BUFF_LEN		131072

typedef std::map< std::string, std::string >	CMapHttpHeader;

struct tagHttpChunk
{
	boost::shared_array< u_int8_t >	pChunk;
	int32_t							nChunkLen; // chunk �ܴ�С, -1 ��ʾ��δ���������ֵ,������ʾ��Чֵ  chunk_size
	u_int32_t						uChunkBuffLen; // ��������� pChunk ����ڴ��ܳ�
	u_int32_t						uChunkHttpLen; // ��������� pChunk �е� http ������,Ҳ�ǵ�ǰ�Ѿ����յ�chunk�ֽ���
	u_int32_t						uChunkDataOffset; // pChunk �� chunk-data ��� pChunk ��ƫ�ƣ�chunk-data �Ĵ�С�ǣ�uChunkHttpLen - uChunkDataOffset

	tagHttpChunk()
	{
		nChunkLen = 0;
		uChunkBuffLen = 0;
		uChunkHttpLen = 0;
		uChunkDataOffset = 0;
	}
};

typedef	std::list< tagHttpChunk >				CLstHttpChunk;


struct tagHttpIncomingPacket
{
	boost::shared_array< u_int8_t >	pPacket;
	u_int32_t						uPacketBuffLen; // ��������� pPacket ����ڴ��ܳ�
	u_int32_t						uPacketHttpLen; // ��������� pPacket �е� http ������,Ҳ�ǵ�ǰ�Ѿ����յ��ֽ���
	u_int32_t						uHeadLen; // http ��ͷ����
	int32_t							nBodyLen; // http ���峤��,�ڽ����������İ�ͷ��,����� content-length,��ֵ����ֶ�, -1 ��ʾ��δ��ֵ

	int32_t							nChunked; // �Ƿ��� chunked, -1 ��ʾ��δ�жϹ�, 0 == false, 1== true     transfer-encoding:chunked
	tagHttpChunk					lstChunk; // http chunk

	int32_t							nConnection; // connection:close ʱ���� 1,connection:���� ʱ���� 2,��δ������ -1

	// ���� http ͷ�������ĸ��ֶ�
	std::string						strVersion;
	std::string						strStatusCode;
	std::string						strReasonPhrase;
	CMapHttpHeader					mapHeader;

	u_int32_t						uMallocSize; // ��������ڴ治��,��Ҫ����Ĵ�С


	tagHttpIncomingPacket()
	{
		uPacketBuffLen = 0;
		uPacketHttpLen = 0;
		uHeadLen = 0;
		nBodyLen = -1;

		nChunked = -1;
		nChunkLen = -1;

		uChunkBuffLen = 0;
		uChunkHttpLen = 0;

		uChunkBuffLen = 0;
		uChunkDataHttpLen = 0;

		nConnection = -1;

		uMallocSize = DEF_RECEIVING_BUFF_LEN;
	}

	const tagHttpIncomingPacket& operator=( const tagHttpIncomingPacket& right )
	{
		pPacket = right.pPacket;
		uPacketBuffLen = right.uPacketBuffLen;
		uPacketHttpLen = right.uPacketHttpLen;
		uHeadLen = right.uHeadLen;
		nBodyLen = right.nBodyLen;

		nChunked = right.nChunked;
		nChunkLen = right.nChunkLen;

		uChunkBuffLen = right.uChunkBuffLen;
		uChunkHttpLen = right.uChunkHttpLen;

		nConnection = right.nConnection;

		uMallocSize = right.uMallocSize;

		return *this;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////
// http ����
struct tagHttpOutgoingPacket
{
	boost::shared_array< u_int8_t > pPacket;
	u_int32_t                       uPacketLen, uPacketSent;

	tagHttpOutgoingPacket()
	{
		uPacketLen = 0;
		uPacketSent = 0;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////
// ��¼һ�� http ��������Ҫ����������
struct tagHttpPacketPair
{
	std::string				strURL; // URL

	std::string				strDomain, strPath; // �� strURL �������

	std::vector< std::string >	vecIP; // ��������������Ľ��
	u_int16_t					u16Port; // �� strURL ����ȡ

	boost::shared_ptr< CTcpClient >	pTcpClient; // ������������ tcp client

	u_int32_t				uPacketId; // ����� packet key ֻ����ƥ�䳬ʱ����ĳ������ȫ���ͳ�ȥ֮�󣬼��볬ʱ����
	tagHttpOutgoingPacket	objOutgoingPacket;
	tagHttpIncomingPacket	objIncomingPacket;

	tagHttpPacketPair()
	{
		u16Port = 0;
		uPacketId = 0;
	}
};

// http ������У�һ�����������ȷ�����У��� epoll_wait �����󣬻ᴦ������е�ȫ������������
typedef std::list< tagHttpPacketPair >						CLstHttpPacketPair;

/////////////////////////////////////////////////////////////////////////////////////////////
// �������Ƕ�ͬһ�� CLstHttpPacketPair ��ӳ��
typedef std::map< u_int32_t, CLstHttpPacketPair::iterator >							CMapTimeout; // packet key -- http packet pair�����ڳ�ʱʱ���ҵ���Ӧ�������
typedef	std::map< boost::shared_ptr< CTcpClient >, CLstHttpPacketPair::iterator > 	CMapClientInfo;// tcp client -- http packet pair��client->��� client ������������

// ���ڱ��� tcpclient �����ü���
typedef std::map< void*, boost::shared_ptr< CTcpClient > >							CMapTcpClient;


/////////////////////////////////////////////////////////////////////////////////////////////
// ��������̰߳�ȫ�ģ�Ҳ�����������Ψһ�ӿ�
class CHttpPacketLayer 
{
	public:
		CHttpPacketLayer();
		virtual ~CHttpPacketLayer();

	public:
		virtual	int32_t	Init( boost::shared_ptr< CEpollEvent > pEpollEvent,
				boost::shared_ptr< IHttpPacketCallback > pPacketCb, 
				boost::shared_ptr< CMemPool > pMemPool, 
				boost::shared_ptr< CPacketId > pPacketId, // packet key ����ֻ��Ϊ�˶�λ��ʱ�İ�������������ƥ�䣬http����ƥ������˳��
				boost::shared_ptr< CTimeoutMgr > pTimeoutMgr, u_int32_t uTimeoutInterval );

		// ��Ҫ���͵İ��ŵ����оͷ���,epoll ���̻߳���epollout��ʱ��ƴ�������ⷢ���������пջ���EAGAIN
		// ������ӻ�δ�������ŵ� CMapHttpConnecting �У��� OnNewTcpClient ��ʱ�򴴽� tcpclient���󶨵� epollevent ��
		// �����ô��� epollevent ����
		virtual	int32_t	SendPacket( const std::string &strURL, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, void* pvParam = NULL); 

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

		// IEpollEventExchange
		virtual	int32_t	OnExchange( void *pThis );

	protected:
		virtual	int32_t	FreeTcpClient( void *pTcpClient );
		virtual	int32_t	FreeHttpPacket( void *pTcpClient ); // �ͷŵ�ǰ pTcpClient ���ڲ����İ�

		// ������ֵ�� 0,��� action
		// action ��ʾ����ȡ���ֶ�����
		// action == 1 ����ȫ��������ϣ������ص����ر�����
		// action == 2 ����ȫ��������ϣ������ص������ǲ��ر�����
		// action == 3 ���廹��Ҫ���� uWant ���ֽ�
		// 		��ʱ����� uWant �� -1����ʾ��֪����Ҫ���ܶ��٣���ô���� DEF_RECEIVING_BUFF_LEN
		virtual	int32_t	ProcessBody( tagHttpIncomingPacket *pIncomingPacket, u_int32_t *puAction, int32_t *pnWant ); // ���� header ��ָ����ʽ���ܡ���ѹ������

		// ������ֵ�� 0,��� action
		// action ��ʾ�������кζ���
		// action == 1 header�������˲��Ҵ������,���Կ�ʼ���������
		// action == 2 header��������,���Ǹ�ʽ����,�����ص�,�ر�����
		// action == 3 header��δ����,��Ҫ�����հ�ͷ
		virtual	int32_t	ProcessHeader( tagHttpIncomingPacket *pIncomingPacket, u_int32_t *puAction );

		//����ͷ��δ��ȫ�����ʣ���ڴ��Ƿ�ﵽ uMalloc
		//����ͷ��ȫ�����ʣ���ڴ��Ƿ�ﵽ uWant����� uWant �� -1�������ж��Ƿ�ﵽ uMalloc
		virtual	int32_t ReallocMemory( tagHttpIncomingPacket *pIncomingPacket );

		// �հ�,���ݵ�ǰ�����ͬ���в�ͬ���հ�����
		virtual	int32_t ReceivePacket( int32_t fd, tagHttpIncomingPacket *pIncomingPacket, int32_t *pnReceivedLen );


	protected:
		boost::recursive_mutex			m_mtxPendingReq; // ���� m_lstPendingReq ʱ���������Ϊֻ����� �������� epoll �߳��ⱻ����

		CLstHttpPacketPair				m_lstPendingReq; // ������������� epoll_wait �������õ���������Ķ������������ɹ���
		CLstHttpPacketPair				m_lstProcessing; // ���ڱ����������

		CMapTcpClient					m_mapTcpClient; // pTcpClient.get() -> pTcpClient�����ڱ������ü���

		// �������ṹ��ָ�� m_lstProcessing �е�Ԫ��
		CMapClientInfo					m_mapClientInfo; // tcp client -> http
		CMapTimeout						m_mapTimeout; // ���ڳ�ʱ���ҵ���ʱ�İ�

		CMapSocketStatus        		m_mapSocketStatus; // �ܶ�Ӧ epoll ��ص� fd ����Ϣ	

		boost::shared_ptr< CEpollEvent >	m_pEpollEvent;
		boost::shared_ptr< CMemPool >		m_pMemPool;
		boost::shared_ptr< CPacketId >		m_pPacketId;

		boost::shared_ptr< CTimeoutMgr >	m_pTimeoutMgr;
		u_int32_t							m_uTimeoutInterval;

		boost::shared_ptr< IHttpPacketCallback >	m_pPacketCallback;


		// ʹ�ûص��ӵ�ԭ�����ڣ���Ҫ���ຯ�����ڲ����� shared_ptr �������࣬��������ڲ��޷��õ� shared_ptr
		boost::shared_ptr< CTcpEventCallbackObj< CHttpPacketLayer >	> 		m_pCallback_ITcpEventCallback;
		boost::shared_ptr< CEpollEventExchangeObj< CHttpPacketLayer > > 	m_pCallback_IEpollEventExchange;
};



#endif
