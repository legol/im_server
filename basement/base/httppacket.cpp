#include "httppacket.h"
#include <netdb.h>
#include <boost/regex.hpp>
#include "misc.h"

// for strcasestr
#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

CHttpPacketLayer::CHttpPacketLayer()
{
	m_pCallback_ITcpEventCallback = boost::shared_ptr< CTcpEventCallbackObj< CHttpPacketLayer > >( new CTcpEventCallbackObj< CHttpPacketLayer >( this ) );
	m_pCallback_ITcpEventCallback->Hook_OnConnected( &CHttpPacketLayer::OnConnected );
	m_pCallback_ITcpEventCallback->Hook_OnConnectionRefused( &CHttpPacketLayer::OnConnectionRefused );
	m_pCallback_ITcpEventCallback->Hook_OnDataIn( &CHttpPacketLayer::OnDataIn );
	m_pCallback_ITcpEventCallback->Hook_OnDataOut( &CHttpPacketLayer::OnDataOut );
	m_pCallback_ITcpEventCallback->Hook_OnNewSocket( &CHttpPacketLayer::OnNewSocket );
	m_pCallback_ITcpEventCallback->Hook_OnClose( &CHttpPacketLayer::OnClose );
	m_pCallback_ITcpEventCallback->Hook_OnError( &CHttpPacketLayer::OnError );
	m_pCallback_ITcpEventCallback->Hook_OnSendPacket( &CHttpPacketLayer::OnSendPacket );
	m_pCallback_ITcpEventCallback->Hook_OnStart( &CHttpPacketLayer::OnStart );

	m_pCallback_IEpollEventExchange = boost::shared_ptr< CEpollEventExchangeObj< CHttpPacketLayer > >( new CEpollEventExchangeObj< CHttpPacketLayer >( this ) );
	m_pCallback_IEpollEventExchange->Hook_OnExchange( &CHttpPacketLayer::OnExchange );
}

CHttpPacketLayer::~CHttpPacketLayer()
{
	m_pCallback_ITcpEventCallback->UnhookAll();
	m_pCallback_IEpollEventExchange->UnhookAll();
}

int32_t	CHttpPacketLayer::Init( boost::shared_ptr< CEpollEvent > pEpollEvent,
		boost::shared_ptr< IHttpPacketCallback > pPacketCb, 
		boost::shared_ptr< CMemPool > pMemPool, 
		boost::shared_ptr< CPacketId > pPacketId, 
		boost::shared_ptr< CTimeoutMgr > pTimeoutMgr, u_int32_t uTimeoutInterval )
{
	if( pEpollEvent.get() == NULL || pPacketCb.get() == NULL || pMemPool.get() == NULL || pPacketId.get() == NULL || pTimeoutMgr.get() == NULL )
	{
		return -1;
	}

	m_pEpollEvent = pEpollEvent;
	m_pPacketCallback = pPacketCb;
	m_pMemPool = pMemPool;
	m_pPacketId = pPacketId;
	m_pTimeoutMgr = pTimeoutMgr;
	m_uTimeoutInterval = uTimeoutInterval;

	// 设置 epoll 交换接口，用于运行时有机会能够增加和减少连接
	m_pEpollEvent->AddExchangeCallback( m_pCallback_IEpollEventExchange );
	return 0;
}

int32_t	CHttpPacketLayer::SendPacket( const std::string &strURL, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, void* pvParam )
{
	boost::recursive_mutex::scoped_lock	lock( m_mtxPendingReq );

	tagHttpPacketPair		objPacket;
	u_int32_t				uPacketId = 0;

	std::string::size_type	pos1, pos2, pos3;

	if( ( NULL == pchPacket.get() ) || (  0   == uPacketLen ) )
	{
		return -1;
	}

	if( m_pPacketId )
	{
		m_pPacketId->GeneratePacketId( &uPacketId );
	}

	objPacket.strURL = strURL;

	objPacket.uPacketId = uPacketId;
	objPacket.objOutgoingPacket.pPacket = pchPacket;
	objPacket.objOutgoingPacket.uPacketLen = uPacketLen;
	objPacket.objOutgoingPacket.uPacketSent = 0;

	// 域名解析, todo: 使用非阻塞的 dns 缓存服务
	char    buf[1024];
	hostent hostinfo, *phost;
	int     ret;

	// 去头
	pos1 = strURL.find( "://" );
	if( pos1 == std::string::npos )
	{
		objPacket.strDomain = strURL;
	}
	else
	{
		objPacket.strDomain = strURL.substr( pos1 + 3 );
	}

	// 去尾
	pos2 = objPacket.strDomain.find( "/" );
	if( pos2 == std::string::npos )
	{
		objPacket.strPath = "/";
	}
	else
	{
		objPacket.strPath = objPacket.strDomain.substr( pos2 + 1 );
		objPacket.strDomain = objPacket.strDomain.substr( 0, pos2 );
	}

	// 去用户名密码
	// todo: 暂不支持

	// 去端口
	pos3 = objPacket.strDomain.find( ":" );
	if( pos3 != std::string::npos )
	{
		objPacket.u16Port = atoi( objPacket.strDomain.substr( pos3 + 1 ).c_str() );
		objPacket.strDomain = objPacket.strDomain.substr( 0, pos3 );
	}
	else
	{
		objPacket.u16Port = 80;
	}

	if( gethostbyname_r( objPacket.strDomain.c_str(), &hostinfo, buf, sizeof(buf), &phost, &ret ) != 0 )
	{
		return -2;
	}

	// 成功获得 IP
	for( int32_t i = 0; hostinfo.h_addr_list[ i ] != NULL; i++ )
	{
		objPacket.vecIP.push_back( inet_ntop( AF_INET, ( void* )( hostinfo.h_addr_list[ i ] ), buf, sizeof( buf ) ) );
	}

	if( m_pPacketCallback )
	{
		if( m_pPacketCallback->OnPreSendPacket( strURL, objPacket.objOutgoingPacket.pPacket, objPacket.objOutgoingPacket.uPacketLen, uPacketId, pvParam ) != 0 )
		{
			return -1;
		}
	}

	m_lstPendingReq.push_back( objPacket );

	return 0;
}

int32_t	CHttpPacketLayer::OnConnected(int32_t fd, sockaddr_in addr, void *pThis)
{
	std::cout << __FUNCTION__ << std::endl;

	CMapTcpClient::iterator			itrClient;
	CMapClientInfo::iterator		itrClientInfo;
	CLstHttpPacketPair::iterator 	itrPacketPair;

	CMapSocketStatus::iterator	itr;

	// 改变 HTTP 状态
	itrClient = m_mapTcpClient.find( pThis );
	if( itrClient == m_mapTcpClient.end() )
	{
		return -1;
	}

	itrClientInfo = m_mapClientInfo.find( itrClient->second );
	if( itrClientInfo == m_mapClientInfo.end() )
	{
		return -1;
	}

	itrPacketPair = itrClientInfo->second;

	// 改变 m_mapSocketStatus 状态
	itr = m_mapSocketStatus.find( fd );
	if( itr == m_mapSocketStatus.end() )
	{
		tagSocketStatus	objSocketStatus;

		objSocketStatus.bConnected = true;
		objSocketStatus.bCanWrite = true;
		objSocketStatus.addr = addr;
		m_mapSocketStatus[ fd ] = objSocketStatus;
		itr = m_mapSocketStatus.find( fd );
	}
	else
	{
		itr->second.bConnected = true;
		itr->second.bCanWrite = true;
		itr->second.addr = addr;
	}

	if( m_pPacketCallback )
	{
		return m_pPacketCallback->OnConnected( fd, addr );
	}

	return 0;
}

int32_t	CHttpPacketLayer::OnConnectionRefused(int32_t fd, void *pThis)
{
	std::cout << __FUNCTION__ << std::endl;
	
	FreeTcpClient( pThis );

	return 0;
}

int32_t	CHttpPacketLayer::OnDataIn( int32_t fd, void *pThis)
{
	std::cout << __FUNCTION__ << std::endl;

	CMapSocketStatus::iterator	itr;
	sockaddr_in					addr;

	CMapTcpClient::iterator			itrClient;
	CMapClientInfo::iterator		itrClientInfo;
	CLstHttpPacketPair::iterator 	itrPacketPair;

	int32_t							nReceivedLen = 0; // 本次接收到多长
	u_int32_t						uHeaderAction, uBodyAction; // 包头和包体处理后的后续动作
	int32_t							nWant; // 还需要接收多长

	tagHttpIncomingPacket			objIncomingPacket;

	itr = m_mapSocketStatus.find( fd );
	if( itr == m_mapSocketStatus.end() )
	{
		tagSocketStatus	objSocketStatus;

		objSocketStatus.bConnected = true;
		objSocketStatus.bCanWrite  = true;
		objSocketStatus.addr       = addr;
		m_mapSocketStatus[ fd ]    = objSocketStatus;
		itr = m_mapSocketStatus.find( fd );
	}
	else
	{
		itr->second.bConnected = true;
		itr->second.bCanWrite  = true;
	}

	addr = itr->second.addr;

	itrClient = m_mapTcpClient.find( pThis );
	if( itrClient == m_mapTcpClient.end() )
	{
		return -1;
	}

	itrClientInfo = m_mapClientInfo.find( itrClient->second );
	if( itrClientInfo == m_mapClientInfo.end() )
	{
		return -1;
	}

	itrPacketPair = itrClientInfo->second;
	objIncomingPacket = ( *itrPacketPair ).objIncomingPacket;

	while( true ) // 一直收到 EAGAIN，或者出错
	{
		// 检查内存是否够本次接收,不过不够,重新分配
		ReallocMemory( &objIncomingPacket );

		// 收包,如果是chunked,会收到 pChunked 里面去
		ReceivePacket( fd, &objIncomingPacket, &nReceivedLen );
		if( 0 == nReceivedLen )
		{
			// 连接被对方断开
			// todo: 发出回调,将当前收到的数据告知应用层

			return -1;
		}

		if( -1 == nReceivedLen )
		{
			if( (EAGAIN == errno) || (EINTR == errno) )
			{
				// 下次再收
				return 0;
			}
			else
			{
				// 出错了
				// todo: 发出回调，将当前收到的数据告知应用层

				return -1;
			}
		}

		if( nReceivedLen < 0 )
		{
			// todo: 回调
			return -1;
		}


		if( objIncomingPacket.uHeadLen == 0 )
		{
			// 包头还未收全

			if( ProcessHeader( &objIncomingPacket, &uHeaderAction ) != 0 )
			{
				// 出错了,关连接
				return -1;
			}
			
			if( uHeaderAction == 1 )
			{
				// action == 1 header收完整了并且处理完毕,可以开始处理包体了
				if( ProcessBody( &objIncomingPacket, &uBodyAction, &nWant ) != 0 )
				{
					// 出错了,关连接
					return -1;
				}

				if( uBodyAction == 1 )
				{
					// action == 1 包体全部处理完毕，发出回调，但是不关闭连接
					// todo:回调
					FreeHttpPacket( pThis );

					continue; // 继续收包,用于长连接逻辑
				}
				else if( uBodyAction == 2 )
				{
					// action == 2 包体全部处理完毕，发出回调，关闭连接
					// todo: 回调
					return -1; // 返回非零会触发 tcpclient 的 close 操作
				}
				else if( uBodyAction == 3 )
				{
					// action == 3 包体还需要接受 nWant 个字节

					continue; // 继续收包
				}
				else
				{
					// 不应该进入这里
					return -1;
				}
			}
			else if( uHeaderAction == 2 )
			{
				// action == 2 header收完整了,但是格式错误,发出回调,关闭链接
				// todo:回调
				return -1;
			}
			else if( uHeaderAction == 3 )
			{
				// action == 3 header还未完整,需要继续收包头
				continue; // 继续收包
			}
			else
			{
				// 不应该进入这里
				return -1;
			}
			
		}
		else
		{
			// 包头已经完整

			if ( ProcessBody( &objIncomingPacket, &uBodyAction, &nWant ) != 0 )
			{
				return -1;
			}

			if( uBodyAction == 1 )
			{
				// action == 1 包体全部处理完毕，发出回调，但是不关闭连接
				// todo:回调
				FreeHttpPacket( pThis );

				continue; // 继续收包,用于长连接逻辑
			}
			else if( uBodyAction == 2 )
			{
				// action == 2 包体全部处理完毕，发出回调，关闭连接
				// todo: 回调
				return -1; // 返回非零会触发 tcpclient 的 close 操作
			}
			else if( uBodyAction == 3 )
			{
				// action == 3 包体还需要接受 nWant 个字节

				continue; // 继续收包
			}
			else
			{
				// 不应该进入这里
				return -1;
			}

		}

	} // while ( true )

	return 0;
}

int32_t	CHttpPacketLayer::OnDataOut( int32_t fd, void *pThis)
{
	std::cout << __FUNCTION__ << std::endl;
	return 0;
}

int32_t	CHttpPacketLayer::OnNewSocket( int32_t fd, void *pThis)
{
	std::cout << __FUNCTION__ << std::endl;

	return 0;
}

int32_t	CHttpPacketLayer::OnClose( int32_t fd, void *pThis)
{
	std::cout << __FUNCTION__ << std::endl;

	FreeTcpClient( pThis );

	return 0;
}

int32_t	CHttpPacketLayer::FreeTcpClient( void *pTcpClient )
{
	CMapTcpClient::iterator			itrClient;
	CMapClientInfo::iterator		itrClientInfo;
	CLstHttpPacketPair::iterator 	itrPacketPair;
	CMapTimeout::iterator			itrTimeout;

	itrClient = m_mapTcpClient.find( pTcpClient );
	if( itrClient != m_mapTcpClient.end() )
	{
		itrClientInfo = m_mapClientInfo.find( itrClient->second );
		if( itrClientInfo != m_mapClientInfo.end() )
		{
			itrPacketPair = itrClientInfo->second;
			itrTimeout = m_mapTimeout.find( ( *itrPacketPair ).uPacketId );
			if( itrTimeout != m_mapTimeout.end() )
			{
				m_mapTimeout.erase( itrTimeout );
			}
		
			m_mapClientInfo.erase( itrClientInfo );
			m_lstProcessing.erase( itrPacketPair );	
		}
	
		m_mapTcpClient.erase( itrClient );
	}

	return 0;	
}


int32_t	CHttpPacketLayer::OnError( int32_t fd, void *pThis )
{
	std::cout << __FUNCTION__ << std::endl;

	FreeTcpClient( pThis );

	return 0;
}

int32_t	CHttpPacketLayer::OnSendPacket( int32_t fd, void *pThis)
{
	CMapSocketStatus::iterator	        itrStatus;

	sockaddr_in                         addr;
	int32_t								nSent;

	itrStatus = m_mapSocketStatus.find( fd );
	if( itrStatus == m_mapSocketStatus.end() )
	{
		// 还未连接
		return 0;
	}

	addr = itrStatus->second.addr;

	if( itrStatus->second.bConnected == false || ( itrStatus->second.bConnected == true && itrStatus->second.bCanWrite == false ) )
	{
		// 还未建立连接
		return 0;
	}

	CMapTcpClient::iterator			itrClient;
	CMapClientInfo::iterator		itrClientInfo;
	CLstHttpPacketPair::iterator 	itrPacketPair;
	CMapTimeout::iterator			itrTimeout;

	itrClient = m_mapTcpClient.find( pThis );
	if( itrClient == m_mapTcpClient.end() )
	{
		return -1;
	}

	itrClientInfo = m_mapClientInfo.find( itrClient->second );
	if( itrClientInfo == m_mapClientInfo.end() )
	{
		return -1;
	}

	itrPacketPair = itrClientInfo->second;
	itrTimeout = m_mapTimeout.find( ( *itrPacketPair ).uPacketId );
	if( itrTimeout != m_mapTimeout.end() )
	{
		return -1;
	}

	while( true )
	{
		nSent = send( fd, (*itrPacketPair).objOutgoingPacket.pPacket.get() + (*itrPacketPair).objOutgoingPacket.uPacketSent, 
				(*itrPacketPair).objOutgoingPacket.uPacketLen - (*itrPacketPair).objOutgoingPacket.uPacketSent, 0  );
		if( nSent == -1 )
		{
			if( (EAGAIN == errno) || ( EINTR == errno) )	
			{
				// 这个 fd 阻塞了
				itrStatus->second.bConnected = true;
				itrStatus->second.bCanWrite = false;
				return 0;
			}
			else
			{
				// 出错，关连接
				itrClient->second->Close( fd );
				return -1;
			}
		}

		if( ( u_int32_t )nSent == (*itrPacketPair).objOutgoingPacket.uPacketLen - (*itrPacketPair).objOutgoingPacket.uPacketSent )
		{
			// 一个逻辑包完整的发送出去了
			( *itrPacketPair) .objOutgoingPacket.uPacketSent = (*itrPacketPair).objOutgoingPacket.uPacketLen;

			if( m_pPacketCallback )
			{
				int32_t			nRet;

				// 加入超时队列
				if( m_pTimeoutMgr )
				{
					m_pTimeoutMgr->AddPacket( ( *itrPacketPair ).objOutgoingPacket.pPacket, (*itrPacketPair).objOutgoingPacket.uPacketLen, 
							fd, (*itrPacketPair).uPacketId, m_uTimeoutInterval, m_pPacketCallback );
				}

				// 发出回调
				nRet = m_pPacketCallback->OnPacketSent( fd, addr, 
						( *itrPacketPair ).objOutgoingPacket.pPacket, (*itrPacketPair).objOutgoingPacket.uPacketLen, 
						(*itrPacketPair).uPacketId );
				if( nRet != 0 )
				{
					return nRet;
				}

			}

			return 0;
		}
		else if ( ( u_int32_t )nSent < (*itrPacketPair).objOutgoingPacket.uPacketLen - (*itrPacketPair).objOutgoingPacket.uPacketSent )
		{
			( *itrPacketPair ).objOutgoingPacket.uPacketSent += nSent;
			continue; // 继续发当前包
		}
		else
		{
			// 不应该进入这里
			return -1;
		}
	}

	return 0;
}

void CHttpPacketLayer::OnStart( void *pThis )
{
	std::cout << __FUNCTION__ << std::endl;
	return;
}

int32_t	CHttpPacketLayer::OnExchange( void *pThis )
{
	do {
		boost::recursive_mutex::scoped_lock	lock( m_mtxPendingReq );
		m_lstProcessing.splice( m_lstProcessing.begin(), m_lstPendingReq );
	} while( 0 );


	boost::shared_ptr< CTcpClient >	pTcpClient;
	for ( CLstHttpPacketPair::iterator itr = m_lstProcessing.begin(); itr != m_lstProcessing.end(); itr++ )	
	{
		if( ( *itr ).pTcpClient.get() != NULL )
		{
			// 来自 m_lstPendingReq 的均处理完毕
			break;
		}

		pTcpClient = g_objPool_TcpClient.Malloc();
		if( pTcpClient.get() == NULL )
		{
			return -1;
		}

		if( ( *itr ).vecIP.empty() )
		{
			// 不应该进入这里
			return -1;
		}

		( *itr ).pTcpClient = pTcpClient;

		m_mapTcpClient[ ( void* )( pTcpClient.get() ) ] = pTcpClient; // 保存住引用计数，在 OnClose/OnError时候释放
		m_mapClientInfo[ pTcpClient ] = itr;

		pTcpClient->Init( m_pEpollEvent, 1000000, m_pCallback_ITcpEventCallback, ( *itr ).vecIP[ 0 ], ( *itr ).u16Port, false, 0, 1 );
	}

	return 0;
}
		
int32_t	CHttpPacketLayer::FreeHttpPacket( void *pTcpClient )
{
	CMapTcpClient::iterator			itrClient;
	CMapClientInfo::iterator		itrClientInfo;
	CLstHttpPacketPair::iterator 	itrPacketPair;
	CMapTimeout::iterator			itrTimeout;

	itrClient = m_mapTcpClient.find( pTcpClient );
	if( itrClient != m_mapTcpClient.end() )
	{
		itrClientInfo = m_mapClientInfo.find( itrClient->second );
		if( itrClientInfo != m_mapClientInfo.end() )
		{
			itrPacketPair = itrClientInfo->second;
			itrTimeout = m_mapTimeout.find( ( *itrPacketPair ).uPacketId );
			if( itrTimeout != m_mapTimeout.end() )
			{
				m_mapTimeout.erase( itrTimeout );
			}
		
			m_mapClientInfo.erase( itrClientInfo );
			m_lstProcessing.erase( itrPacketPair );	
		}
	}

	return 0;
}
	
int32_t	CHttpPacketLayer::ProcessBody( tagHttpIncomingPacket *pIncomingPacket, u_int32_t *puAction, int32_t *pnWant )
{
	CMapHttpHeader::iterator	itr;

	if( pIncomingPacket == NULL || puAction == NULL || pnWant == NULL )
	{
		return -1;
	}

	if( pIncomingPacket->nChunked == -1 )
	{
		// 不应该进入这里
		return -1;
	}
	else if( pIncomingPacket->nChunked == 0 )
	{
		// todo:
		// 检查是否包体已经接收全了
		if( pIncomingPacket->nBodyLen <= 0 )
		{
			// todo: 这种情况如何判断包长呢?
			*puAction = 3;
			*pnWant = -1;
			
			return 0;
		}

		if( pIncomingPacket->uPacketHttpLen >= pIncomingPacket->uHeadLen + ( u_int32_t )( pIncomingPacket->nBodyLen ) )
		{
			if( pIncomingPacket->nConnection == 1 )
			{
				*puAction = 1;
				return 0;
			}
			else if( pIncomingPacket->nConnection == 2 )
			{
				*puAction = 2;
				return 0;
			}
			else
			{
				// 不应该进入这里
				*puAction = 1;
				return 0;
			}

			return 0;
		}
		else
		{
			*puAction = 3;
			*pnWant = ( int32_t )( pIncomingPacket->uHeadLen ) + pIncomingPacket->nBodyLen - ( int32_t )( pIncomingPacket->uPacketHttpLen );
			return 0;
		}
	}
	else // nChunked == 1
	{
		// Chunk 的数据结构,来自 RFC2616
		//Chunked-Body = *chunk
		//last-chunk
		//trailer
		//CRLF
		//
		//chunk = chunk-size [ chunk-extension ] CRLF
		//chunk-data CRLF
		//chunk-size = 1*HEX
		//last-chunk = 1*("0") [ chunk-extension ] CRLF
		//
		//chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
		//chunk-ext-name = token
		//chunk-ext-val = token | quoted-string
		//chunk-data = chunk-size(OCTET)
		//trailer = *(entity-header CRLF)
		
	
		// 先将 pPacket 中超出 uHeadLen 的部分拷贝到 pChunk 里面
		if( pIncomingPacket->uPacketHttpLen > pIncomingPacket->uHeadLen )
		{
			if( pIncomingPacket->pChunk.get() == NULL )
			{
				while( pIncomingPacket->uMallocSize <= pIncomingPacket->uPacketHttpLen - pIncomingPacket->uHeadLen )
				{
					pIncomingPacket->uMallocSize <<= 1;
				}

				pIncomingPacket->pChunk = m_pMemPool->Malloc( pIncomingPacket->uMallocSize );
				if( pIncomingPacket->pChunk == NULL )
				{
					return -1;
				}
				pIncomingPacket->uChunkBuffLen = pIncomingPacket->uMallocSize;
				pIncomingPacket->uChunkHttpLen = 0;
				pIncomingPacket->uMallocSize <<= 1;

				memcpy( pIncomingPacket->pChunk.get(), 
							pIncomingPacket->pPacket.get() + pIncomingPacket->uHeadLen, 
							pIncomingPacket->uPacketHttpLen - pIncomingPacket->uHeadLen );

				pIncomingPacket->uChunkHttpLen = pIncomingPacket->uPacketHttpLen - pIncomingPacket->uHeadLen;
				pIncomingPacket->uPacketHttpLen = pIncomingPacket->uHeadLen;

				*( pIncomingPacket->pChunk.get() + pIncomingPacket->uChunkHttpLen ) = '\0';
			}
			else
			{
				// 不可能进入这里,因为 uPacketHttpLen > uHeadLen 只可能是第一次进入 chunk 逻辑, 这时会把超出的这些字节拷贝到 pChunk,并且让 uPacketHttpLen = uHeadLen
				return -1;
			}

		}
		
		if( pIncomingPacket->uPacketHttpLen < pIncomingPacket->uHeadLen )
		{
			// 不可能进入这里
			return -1;
		}

		// 读取 chunk-size 和 chunk-extension
		if( pIncomingPacket->nChunkLen == -1 )
		{
			char		*pstrCRLF, *pstrSpace;
			std::string	strChunkFirstLine, strChunkLen;

			pstrCRLF = strstr( ( char* )( pIncomingPacket->pChunk.get() ), "\r\n" );
			if( pstrCRLF == NULL )
			{
				// chunk 的第一行还未读来
				*puAction = 3;
				*pnWant = -1;
				return 0;
			}

			strChunkFirstLine.assign( ( char* )( pIncomingPacket->pChunk.get() ), pstrCRLF - ( char* )( pIncomingPacket->pChunk.get() ) + 2 );
			pstrSpace = strstr( strChunkFirstLine.c_str(), " " );
			if( pstrSpace == NULL )
			{
				// chunk 格式错误
				return -1;
			}

			// 解析 chunk-size
			strChunkLen = strChunkFirstLine.substr( 0, pstrSpacePos - strChunkFirstLine.c_str() );
			pIncomingPacket->nChunkLen = strtol( strChunkLen.c_str(), NULL, 16 );

			pIncomingPacket->uChunkDataOffset = ( u_int32_t )( pstrCRLF - ( char* )( pIncomingPacket->pChunk.get() ) ) + 2;

			// todo: 读取 chunk-extension
		}

		// 判断当前 chunk 是否读取完整
		if( pIncomingPacket->nChunkLen > 0 )
		{
			
		}
		else
		{
			// 不应该进入这里
			return -1;
		}
	}

	return 0;
}
		
int32_t	CHttpPacketLayer::ProcessHeader( tagHttpIncomingPacket *pIncomingPacket, u_int32_t *puAction )
{
	// 检查是否包头收全了
	char						*pstrDelimiterPos;
	std::string					strHeader, strKey, strValue;
	std::string::size_type		pos;
	CMapHttpHeader::iterator	itrHeader;

	if( pIncomingPacket == NULL || puAction == NULL )
	{
		return -1;
	}
	*puAction = 0;

	if( pIncomingPacket->pPacket.get() == NULL || pIncomingPacket->uPacketHttpLen < 4 ) // 至少得包含 "\r\n\r\n"
	{
		return -1;
	}

	// 注意,这里要求 pIncomingPacket->pPacket 一定是零结尾
	pstrDelimiterPos = strstr( ( char* )( pIncomingPacket->pPacket.get() ), "\r\n\r\n" );
	if( pstrDelimiterPos == NULL )
	{
		// 还未收全包头,继续收
		*puAction = 3;
		return 0;
	}

	// 包头长度
	pIncomingPacket->uHeadLen = ( unsigned int32_t )( ( pstrDelimiterPos + 4 ) - ( char* )( pIncomingPacket->pPacket.get() ) );
	strHeader.assign( ( char* )( pIncomingPacket->pPacket.get() ), pIncomingPacket->uHeadLen );

	std::cout << "---------header----------" << std::endl;
	std::cout << strHeader << std::endl;
	std::cout << "-------------------------" << std::endl;

	// 解析 http 头
	boost::regex 	regexFirstLine( "(.+?)\\s+(.+?)\\s+(.+?)\\s*$", boost::regbase::icase );
	boost::regex 	regexParam( "\\s*(.+?)\\s*:\\s*(.+?)\\s*$", boost::regbase::icase );
	boost::smatch	matchesFirstLine, matchesParam;
	std::string::const_iterator itrBegin, itrEnd;
	
	// 先解析第一行
	pos = strHeader.find( "\r\n" );
	if( pos == std::string::npos )
	{
		// http 格式错误
		// todo: 发出回调
		*puAction = 2;
		return 0;
	}

	itrBegin = strHeader.begin();
	itrEnd = ( itrBegin + pos + 2 ); // +2 是为了包含进 "\r\n",现在的 itrEnd 指向 \r\n 后的第一个字符
	if ( boost::regex_search( itrBegin, itrEnd, matchesFirstLine, regexFirstLine ) )
	{
		pIncomingPacket->strVersion = matchesFirstLine[ 1 ];
		pIncomingPacket->strStatusCode = matchesFirstLine[ 2 ];
		pIncomingPacket->strReasonPhrase = matchesFirstLine[ 3 ];

		std::cout << "-----header first line---" << std::endl;
		std::cout << pIncomingPacket->strVersion << std::endl;
		std::cout << pIncomingPacket->strStatusCode << std::endl;
		std::cout << pIncomingPacket->strReasonPhrase << std::endl;
		std::cout << "-------------------------" << std::endl;
	}
	else
	{
		// http 响应格式错误
		*puAction = 2;
		return 0;
	}

	std::cout << "------header params------" << std::endl;

	// 解析后面的键值对
	itrBegin = itrEnd;
	itrEnd = strHeader.end();
	while( boost::regex_search( itrBegin, itrEnd, matchesParam, regexParam ) )
	{
		strKey = matchesParam[ 1 ];
		strValue = matchesParam[ 2 ];

		// key 转小写	
		strlwr( const_cast< char* >( strKey.c_str() ), strKey.length() );
		pIncomingPacket->mapHeader[ strKey ] = strValue;

		std::cout << strKey << ":" << strValue << std::endl;

		itrBegin = matchesParam[ 0 ].second;
	}
	std::cout << "-------------------------" << std::endl;

	if( pIncomingPacket->nChunked == -1 )
	{
		itrHeader = pIncomingPacket->mapHeader.find( "transfer-encoding" );
		if( itrHeader == pIncomingPacket->mapHeader.end() )
		{
			pIncomingPacket->nChunked = 0;
		}
		else
		{
			if( strcasecmp( itrHeader->second.c_str(), "chunked" ) == 0 )
			{
				pIncomingPacket->nChunked = 1;
			}
			else
			{
				pIncomingPacket->nChunked = 0;
			}
		}
	}

	if( pIncomingPacket->nBodyLen == -1 )
	{
		itrHeader = pIncomingPacket->mapHeader.find( "content-length" );
		if( itrHeader == pIncomingPacket->mapHeader.end() )
		{
			pIncomingPacket->nBodyLen = 0;
		}
		else
		{
			pIncomingPacket->nBodyLen = ( u_int32_t )atoi( itrHeader->second.c_str() );
		}
	}

	if( pIncomingPacket->nConnection == -1 )
	{
		itrHeader = pIncomingPacket->mapHeader.find( "connection" );
		if( itrHeader == pIncomingPacket->mapHeader.end() )
		{
			pIncomingPacket->nConnection = 2;
		}
		else
		{
			if( strcasecmp( itrHeader->second.c_str(), "close" ) == 0 )
			{
				pIncomingPacket->nConnection = 1;
			}
			else
			{
				pIncomingPacket->nConnection = 2;
			}
		}
	}

	*puAction = 1;
	return 0;
}
		
int32_t CHttpPacketLayer::ReallocMemory( tagHttpIncomingPacket *pIncomingPacket )
{
	if( pIncomingPacket->uHeadLen == 0 )
	{
		// 检查剩余内存是否够本次接收操作
		if( pIncomingPacket->uPacketHttpLen + DEF_RECEIVING_BUFF_LEN > pIncomingPacket->uPacketBuffLen )
		{
			boost::shared_array< u_int8_t >	pNewBuff;
	
			pNewBuff = m_pMemPool->Malloc( pIncomingPacket->uMallocSize );
			if( pNewBuff.get() == NULL )
			{
				std::cerr << "outof_mem " << __FILE__ << ":" << __LINE__ << std::endl;
				return 0;
			}
	
			// 拷贝旧内存过来
			if( pIncomingPacket->pPacket.get() != NULL && pIncomingPacket->uPacketHttpLen != 0 )
			{
				memcpy( pNewBuff.get(), pIncomingPacket->pPacket.get(), pIncomingPacket->uPacketHttpLen );
			}
			
			pIncomingPacket->pPacket = pNewBuff;
			pIncomingPacket->uPacketBuffLen = pIncomingPacket->uMallocSize;
	
			// 下次将分配两倍的内存
			pIncomingPacket->uMallocSize = pIncomingPacket->uMallocSize << 1;
		}
	}
	else
	{
		if( pIncomingPacket->nChunked == -1 )
		{
			// 不应该进入这里
			return -1;
		}
		else if( pIncomingPacket->nChunked == 0 )
		{
			// 检查剩余内存是否够本次接收操作
			if( pIncomingPacket->uPacketHttpLen + DEF_RECEIVING_BUFF_LEN > pIncomingPacket->uPacketBuffLen )
			{
				boost::shared_array< u_int8_t >	pNewBuff;
		
				pNewBuff = m_pMemPool->Malloc( pIncomingPacket->uMallocSize );
				if( pNewBuff.get() == NULL )
				{
					std::cerr << "outof_mem " << __FILE__ << ":" << __LINE__ << std::endl;
					return 0;
				}
		
				// 拷贝旧内存过来
				if( pIncomingPacket->pPacket.get() != NULL && pIncomingPacket->uPacketHttpLen != 0 )
				{
					memcpy( pNewBuff.get(), pIncomingPacket->pPacket.get(), pIncomingPacket->uPacketHttpLen );
				}
				
				pIncomingPacket->pPacket = pNewBuff;
				pIncomingPacket->uPacketBuffLen = pIncomingPacket->uMallocSize;
		
				// 下次将分配两倍的内存
				pIncomingPacket->uMallocSize = pIncomingPacket->uMallocSize << 1;
			}
		}
		else // nChunked == 1
		{
			// 检查剩余内存是否够本次接收操作
			if( pIncomingPacket->uChunkHttpLen + DEF_RECEIVING_BUFF_LEN > pIncomingPacket->uChunkBuffLen )
			{
				boost::shared_array< u_int8_t >	pNewBuff;
		
				pNewBuff = m_pMemPool->Malloc( pIncomingPacket->uMallocSize );
				if( pNewBuff.get() == NULL )
				{
					std::cerr << "outof_mem " << __FILE__ << ":" << __LINE__ << std::endl;
					return 0;
				}
		
				// 拷贝旧内存过来
				if( pIncomingPacket->pChunk.get() != NULL && pIncomingPacket->uChunkHttpLen != 0 )
				{
					memcpy( pNewBuff.get(), pIncomingPacket->pChunk.get(), pIncomingPacket->uChunkHttpLen );
				}
				
				pIncomingPacket->pChunk = pNewBuff;
				pIncomingPacket->uChunkBuffLen = pIncomingPacket->uMallocSize;
		
				// 下次将分配两倍的内存
				pIncomingPacket->uMallocSize = pIncomingPacket->uMallocSize << 1;
			}
		}
	}
	

	return 0;
}
		
int32_t CHttpPacketLayer::ReceivePacket( int32_t fd, tagHttpIncomingPacket *pIncomingPacket, int32_t *pnReceivedLen )
{
	if( pIncomingPacket == NULL || pnReceivedLen == 0 )
	{
		return -1;
	}
	
	if( pIncomingPacket->uHeadLen == 0 )
	{
		*pnReceivedLen = recv( fd, 
				pIncomingPacket->pPacket.get() + pIncomingPacket->uPacketHttpLen, 
				DEF_RECEIVING_BUFF_LEN - 1, // -1 是为了最后流出一个字节填充 \0
				0 );

		if( *pnReceivedLen > 0 )
		{
			pIncomingPacket->uPacketHttpLen += *pnReceivedLen;
			*( char* )( pIncomingPacket->pPacket.get() + pIncomingPacket->uPacketHttpLen ) = '\0'; // 0 结尾它,因为 strstr 函数需要输入的是 0 结尾的字符串
		}
	}
	else
	{
		if( pIncomingPacket->nChunked == -1 )
		{
			// 不应该进入这里
			return -1;
		}
		else if( pIncomingPacket->nChunked == 0 )
		{
			*pnReceivedLen = recv( fd, 
					pIncomingPacket->pPacket.get() + pIncomingPacket->uPacketHttpLen, 
					DEF_RECEIVING_BUFF_LEN - 1, // -1 是为了最后流出一个字节填充 \0
					0 );

			if( *pnReceivedLen > 0 )
			{
				pIncomingPacket->uPacketHttpLen += *pnReceivedLen;
				*( char* )( pIncomingPacket->pPacket.get() + pIncomingPacket->uPacketHttpLen ) = '\0'; // 0 结尾它,因为 strstr 函数需要输入的是 0 结尾的字符串
			}

			return 0;
		}
		else // nChunked == 1
		{
			// todo:
		}
	}

	return -1;	
}


