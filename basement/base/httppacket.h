#ifndef _HTTP_PACKET_INCLUDED_
#define _HTTP_PACKET_INCLUDED_

// http 包的收发时候组包逻辑
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
		// 收到一个完整的逻辑包后被调用
		virtual	int32_t	OnPacketReceived( int32_t fd, sockaddr_in addr, 
				boost::shared_array< u_int8_t > pchHead, u_int32_t uHeadLen, // 包括 \r\n\r\n
				boost::shared_array< u_int8_t > pchBody, u_int32_t uBodyLen, 
				u_int32_t uPacketId ){ return -1; }; 

		// 即将发送一个包之前被调用，在这个回调里面可以得到底层分配的 packet_id ，这样在将来收到打包的时候可以知道是哪个请求包的打包
		virtual	int32_t	OnPreSendPacket( const std::string &strURL, 
				boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId, void* pvParam = NULL){ return 0; }; // todo return -1

		// 发出一个完整的逻辑包
		virtual	int32_t	OnPacketSent( int32_t fd, sockaddr_in addr, 
				boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen,		
				u_int32_t uPacketId ){ return 0; }; // todo return -1 

		// 超时
		virtual	int32_t OnTimeout( boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ){ return -1; };

		// 将 ITcpEventCallback 透传到这里
		virtual int32_t	OnConnected(int32_t fd, sockaddr_in addr) { return -1; };
		virtual int32_t	OnConnectionRefused(int32_t fd, sockaddr_in addr) { return -1; };
		virtual int32_t	OnClose(int32_t fd, sockaddr_in addr) { return -1; };
		virtual int32_t	OnError(int32_t fd, sockaddr_in addr) { return -1; };
		virtual	void	OnStart( void ) {};
};


/////////////////////////////////////////////////////////////////////////////////////////////
// http 收包
// 128K 的初始接收缓冲区大小,每次缓冲大小不够的时候,重新分配当前大小 2 倍的缓冲区
#define DEF_RECEIVING_BUFF_LEN		131072

typedef std::map< std::string, std::string >	CMapHttpHeader;

struct tagHttpChunk
{
	boost::shared_array< u_int8_t >	pChunk;
	int32_t							nChunkLen; // chunk 总大小, -1 表示还未解析出这个值,其他表示有效值  chunk_size
	u_int32_t						uChunkBuffLen; // 这个长度是 pChunk 这块内存总长
	u_int32_t						uChunkHttpLen; // 这个长度是 pChunk 中的 http 包长度,也是当前已经接收的chunk字节数
	u_int32_t						uChunkDataOffset; // pChunk 中 chunk-data 相对 pChunk 的偏移，chunk-data 的大小是，uChunkHttpLen - uChunkDataOffset

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
	u_int32_t						uPacketBuffLen; // 这个长度是 pPacket 这块内存总长
	u_int32_t						uPacketHttpLen; // 这个长度是 pPacket 中的 http 包长度,也是当前已经接收的字节数
	u_int32_t						uHeadLen; // http 包头长度
	int32_t							nBodyLen; // http 包体长度,在接收了完整的包头后,会解析 content-length,赋值这个字段, -1 表示还未赋值

	int32_t							nChunked; // 是否是 chunked, -1 表示还未判断过, 0 == false, 1== true     transfer-encoding:chunked
	tagHttpChunk					lstChunk; // http chunk

	int32_t							nConnection; // connection:close 时候是 1,connection:其他 时候是 2,还未解析是 -1

	// 解析 http 头生成这四个字段
	std::string						strVersion;
	std::string						strStatusCode;
	std::string						strReasonPhrase;
	CMapHttpHeader					mapHeader;

	u_int32_t						uMallocSize; // 如果发生内存不足,需要分配的大小


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
// http 发包
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
// 记录一次 http 交互所需要的所有数据
struct tagHttpPacketPair
{
	std::string				strURL; // URL

	std::string				strDomain, strPath; // 从 strURL 解析获得

	std::vector< std::string >	vecIP; // 经过域名解析后的结果
	u_int16_t					u16Port; // 从 strURL 中提取

	boost::shared_ptr< CTcpClient >	pTcpClient; // 处理这个请求的 tcp client

	u_int32_t				uPacketId; // 这里的 packet key 只用来匹配超时，当某个包完全发送出去之后，加入超时队列
	tagHttpOutgoingPacket	objOutgoingPacket;
	tagHttpIncomingPacket	objIncomingPacket;

	tagHttpPacketPair()
	{
		u16Port = 0;
		uPacketId = 0;
	}
};

// http 请求队列，一次请求到来会先放入队列，等 epoll_wait 出来后，会处理队列中的全部待处理数据
typedef std::list< tagHttpPacketPair >						CLstHttpPacketPair;

/////////////////////////////////////////////////////////////////////////////////////////////
// 这两个是对同一个 CLstHttpPacketPair 的映射
typedef std::map< u_int32_t, CLstHttpPacketPair::iterator >							CMapTimeout; // packet key -- http packet pair，用于超时时候找到对应的请求包
typedef	std::map< boost::shared_ptr< CTcpClient >, CLstHttpPacketPair::iterator > 	CMapClientInfo;// tcp client -- http packet pair，client->这个 client 正在做的事情

// 用于保存 tcpclient 的引用计数
typedef std::map< void*, boost::shared_ptr< CTcpClient > >							CMapTcpClient;


/////////////////////////////////////////////////////////////////////////////////////////////
// 这个类是线程安全的，也是网络层对外的唯一接口
class CHttpPacketLayer 
{
	public:
		CHttpPacketLayer();
		virtual ~CHttpPacketLayer();

	public:
		virtual	int32_t	Init( boost::shared_ptr< CEpollEvent > pEpollEvent,
				boost::shared_ptr< IHttpPacketCallback > pPacketCb, 
				boost::shared_ptr< CMemPool > pMemPool, 
				boost::shared_ptr< CPacketId > pPacketId, // packet key 存在只是为了定位超时的包，并不用来做匹配，http包的匹配依靠顺序
				boost::shared_ptr< CTimeoutMgr > pTimeoutMgr, u_int32_t uTimeoutInterval );

		// 将要发送的包放到队列就返回,epoll 的线程会在epollout的时候“拼命”向外发，发到队列空或者EAGAIN
		// 如果连接还未建立，放到 CMapHttpConnecting 中，在 OnNewTcpClient 的时候创建 tcpclient，绑定到 epollevent 上
		// 这样好处是 epollevent 无锁
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
		virtual	int32_t	FreeHttpPacket( void *pTcpClient ); // 释放当前 pTcpClient 正在操作的包

		// 当返回值是 0,检查 action
		// action 表示随后采取何种动作，
		// action == 1 包体全部处理完毕，发出回调，关闭连接
		// action == 2 包体全部处理完毕，发出回调，但是不关闭连接
		// action == 3 包体还需要接受 uWant 个字节
		// 		此时，如果 uWant 是 -1，表示不知道需要接受多少，那么接受 DEF_RECEIVING_BUFF_LEN
		virtual	int32_t	ProcessBody( tagHttpIncomingPacket *pIncomingPacket, u_int32_t *puAction, int32_t *pnWant ); // 按照 header 的指定方式接受、解压缩包体

		// 当返回值是 0,检查 action
		// action 表示后续进行何动作
		// action == 1 header收完整了并且处理完毕,可以开始处理包体了
		// action == 2 header收完整了,但是格式错误,发出回调,关闭链接
		// action == 3 header还未完整,需要继续收包头
		virtual	int32_t	ProcessHeader( tagHttpIncomingPacket *pIncomingPacket, u_int32_t *puAction );

		//当包头还未收全，检查剩余内存是否达到 uMalloc
		//当包头收全，检查剩余内存是否达到 uWant，如果 uWant 是 -1，则还是判断是否达到 uMalloc
		virtual	int32_t ReallocMemory( tagHttpIncomingPacket *pIncomingPacket );

		// 收包,根据当前情况不同会有不同的收包动作
		virtual	int32_t ReceivePacket( int32_t fd, tagHttpIncomingPacket *pIncomingPacket, int32_t *pnReceivedLen );


	protected:
		boost::recursive_mutex			m_mtxPendingReq; // 操作 m_lstPendingReq 时候枷锁，因为只有这个 变量会在 epoll 线程外被操作

		CLstHttpPacketPair				m_lstPendingReq; // 待处理的请求，在 epoll_wait 出来后会得到处理，这里的都是域名解析成功的
		CLstHttpPacketPair				m_lstProcessing; // 正在被处理的请求

		CMapTcpClient					m_mapTcpClient; // pTcpClient.get() -> pTcpClient，用于保存引用计数

		// 这两个结构都指向 m_lstProcessing 中的元素
		CMapClientInfo					m_mapClientInfo; // tcp client -> http
		CMapTimeout						m_mapTimeout; // 用于超时后，找到超时的包

		CMapSocketStatus        		m_mapSocketStatus; // 受对应 epoll 监控的 fd 的信息	

		boost::shared_ptr< CEpollEvent >	m_pEpollEvent;
		boost::shared_ptr< CMemPool >		m_pMemPool;
		boost::shared_ptr< CPacketId >		m_pPacketId;

		boost::shared_ptr< CTimeoutMgr >	m_pTimeoutMgr;
		u_int32_t							m_uTimeoutInterval;

		boost::shared_ptr< IHttpPacketCallback >	m_pPacketCallback;


		// 使用回调子的原因在于，需要在类函数的内部传递 shared_ptr 给其他类，而在类的内部无法拿到 shared_ptr
		boost::shared_ptr< CTcpEventCallbackObj< CHttpPacketLayer >	> 		m_pCallback_ITcpEventCallback;
		boost::shared_ptr< CEpollEventExchangeObj< CHttpPacketLayer > > 	m_pCallback_IEpollEventExchange;
};



#endif
