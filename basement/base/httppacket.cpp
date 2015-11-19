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

	// ���� epoll �����ӿڣ���������ʱ�л����ܹ����Ӻͼ�������
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

	// ��������, todo: ʹ�÷������� dns �������
	char    buf[1024];
	hostent hostinfo, *phost;
	int     ret;

	// ȥͷ
	pos1 = strURL.find( "://" );
	if( pos1 == std::string::npos )
	{
		objPacket.strDomain = strURL;
	}
	else
	{
		objPacket.strDomain = strURL.substr( pos1 + 3 );
	}

	// ȥβ
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

	// ȥ�û�������
	// todo: �ݲ�֧��

	// ȥ�˿�
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

	// �ɹ���� IP
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

	// �ı� HTTP ״̬
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

	// �ı� m_mapSocketStatus ״̬
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

	int32_t							nReceivedLen = 0; // ���ν��յ��೤
	u_int32_t						uHeaderAction, uBodyAction; // ��ͷ�Ͱ��崦���ĺ�������
	int32_t							nWant; // ����Ҫ���ն೤

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

	while( true ) // һֱ�յ� EAGAIN�����߳���
	{
		// ����ڴ��Ƿ񹻱��ν���,��������,���·���
		ReallocMemory( &objIncomingPacket );

		// �հ�,�����chunked,���յ� pChunked ����ȥ
		ReceivePacket( fd, &objIncomingPacket, &nReceivedLen );
		if( 0 == nReceivedLen )
		{
			// ���ӱ��Է��Ͽ�
			// todo: �����ص�,����ǰ�յ������ݸ�֪Ӧ�ò�

			return -1;
		}

		if( -1 == nReceivedLen )
		{
			if( (EAGAIN == errno) || (EINTR == errno) )
			{
				// �´�����
				return 0;
			}
			else
			{
				// ������
				// todo: �����ص�������ǰ�յ������ݸ�֪Ӧ�ò�

				return -1;
			}
		}

		if( nReceivedLen < 0 )
		{
			// todo: �ص�
			return -1;
		}


		if( objIncomingPacket.uHeadLen == 0 )
		{
			// ��ͷ��δ��ȫ

			if( ProcessHeader( &objIncomingPacket, &uHeaderAction ) != 0 )
			{
				// ������,������
				return -1;
			}
			
			if( uHeaderAction == 1 )
			{
				// action == 1 header�������˲��Ҵ������,���Կ�ʼ���������
				if( ProcessBody( &objIncomingPacket, &uBodyAction, &nWant ) != 0 )
				{
					// ������,������
					return -1;
				}

				if( uBodyAction == 1 )
				{
					// action == 1 ����ȫ��������ϣ������ص������ǲ��ر�����
					// todo:�ص�
					FreeHttpPacket( pThis );

					continue; // �����հ�,���ڳ������߼�
				}
				else if( uBodyAction == 2 )
				{
					// action == 2 ����ȫ��������ϣ������ص����ر�����
					// todo: �ص�
					return -1; // ���ط���ᴥ�� tcpclient �� close ����
				}
				else if( uBodyAction == 3 )
				{
					// action == 3 ���廹��Ҫ���� nWant ���ֽ�

					continue; // �����հ�
				}
				else
				{
					// ��Ӧ�ý�������
					return -1;
				}
			}
			else if( uHeaderAction == 2 )
			{
				// action == 2 header��������,���Ǹ�ʽ����,�����ص�,�ر�����
				// todo:�ص�
				return -1;
			}
			else if( uHeaderAction == 3 )
			{
				// action == 3 header��δ����,��Ҫ�����հ�ͷ
				continue; // �����հ�
			}
			else
			{
				// ��Ӧ�ý�������
				return -1;
			}
			
		}
		else
		{
			// ��ͷ�Ѿ�����

			if ( ProcessBody( &objIncomingPacket, &uBodyAction, &nWant ) != 0 )
			{
				return -1;
			}

			if( uBodyAction == 1 )
			{
				// action == 1 ����ȫ��������ϣ������ص������ǲ��ر�����
				// todo:�ص�
				FreeHttpPacket( pThis );

				continue; // �����հ�,���ڳ������߼�
			}
			else if( uBodyAction == 2 )
			{
				// action == 2 ����ȫ��������ϣ������ص����ر�����
				// todo: �ص�
				return -1; // ���ط���ᴥ�� tcpclient �� close ����
			}
			else if( uBodyAction == 3 )
			{
				// action == 3 ���廹��Ҫ���� nWant ���ֽ�

				continue; // �����հ�
			}
			else
			{
				// ��Ӧ�ý�������
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
		// ��δ����
		return 0;
	}

	addr = itrStatus->second.addr;

	if( itrStatus->second.bConnected == false || ( itrStatus->second.bConnected == true && itrStatus->second.bCanWrite == false ) )
	{
		// ��δ��������
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
				// ��� fd ������
				itrStatus->second.bConnected = true;
				itrStatus->second.bCanWrite = false;
				return 0;
			}
			else
			{
				// ����������
				itrClient->second->Close( fd );
				return -1;
			}
		}

		if( ( u_int32_t )nSent == (*itrPacketPair).objOutgoingPacket.uPacketLen - (*itrPacketPair).objOutgoingPacket.uPacketSent )
		{
			// һ���߼��������ķ��ͳ�ȥ��
			( *itrPacketPair) .objOutgoingPacket.uPacketSent = (*itrPacketPair).objOutgoingPacket.uPacketLen;

			if( m_pPacketCallback )
			{
				int32_t			nRet;

				// ���볬ʱ����
				if( m_pTimeoutMgr )
				{
					m_pTimeoutMgr->AddPacket( ( *itrPacketPair ).objOutgoingPacket.pPacket, (*itrPacketPair).objOutgoingPacket.uPacketLen, 
							fd, (*itrPacketPair).uPacketId, m_uTimeoutInterval, m_pPacketCallback );
				}

				// �����ص�
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
			continue; // ��������ǰ��
		}
		else
		{
			// ��Ӧ�ý�������
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
			// ���� m_lstPendingReq �ľ��������
			break;
		}

		pTcpClient = g_objPool_TcpClient.Malloc();
		if( pTcpClient.get() == NULL )
		{
			return -1;
		}

		if( ( *itr ).vecIP.empty() )
		{
			// ��Ӧ�ý�������
			return -1;
		}

		( *itr ).pTcpClient = pTcpClient;

		m_mapTcpClient[ ( void* )( pTcpClient.get() ) ] = pTcpClient; // ����ס���ü������� OnClose/OnErrorʱ���ͷ�
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
		// ��Ӧ�ý�������
		return -1;
	}
	else if( pIncomingPacket->nChunked == 0 )
	{
		// todo:
		// ����Ƿ�����Ѿ�����ȫ��
		if( pIncomingPacket->nBodyLen <= 0 )
		{
			// todo: �����������жϰ�����?
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
				// ��Ӧ�ý�������
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
		// Chunk �����ݽṹ,���� RFC2616
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
		
	
		// �Ƚ� pPacket �г��� uHeadLen �Ĳ��ֿ����� pChunk ����
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
				// �����ܽ�������,��Ϊ uPacketHttpLen > uHeadLen ֻ�����ǵ�һ�ν��� chunk �߼�, ��ʱ��ѳ�������Щ�ֽڿ����� pChunk,������ uPacketHttpLen = uHeadLen
				return -1;
			}

		}
		
		if( pIncomingPacket->uPacketHttpLen < pIncomingPacket->uHeadLen )
		{
			// �����ܽ�������
			return -1;
		}

		// ��ȡ chunk-size �� chunk-extension
		if( pIncomingPacket->nChunkLen == -1 )
		{
			char		*pstrCRLF, *pstrSpace;
			std::string	strChunkFirstLine, strChunkLen;

			pstrCRLF = strstr( ( char* )( pIncomingPacket->pChunk.get() ), "\r\n" );
			if( pstrCRLF == NULL )
			{
				// chunk �ĵ�һ�л�δ����
				*puAction = 3;
				*pnWant = -1;
				return 0;
			}

			strChunkFirstLine.assign( ( char* )( pIncomingPacket->pChunk.get() ), pstrCRLF - ( char* )( pIncomingPacket->pChunk.get() ) + 2 );
			pstrSpace = strstr( strChunkFirstLine.c_str(), " " );
			if( pstrSpace == NULL )
			{
				// chunk ��ʽ����
				return -1;
			}

			// ���� chunk-size
			strChunkLen = strChunkFirstLine.substr( 0, pstrSpacePos - strChunkFirstLine.c_str() );
			pIncomingPacket->nChunkLen = strtol( strChunkLen.c_str(), NULL, 16 );

			pIncomingPacket->uChunkDataOffset = ( u_int32_t )( pstrCRLF - ( char* )( pIncomingPacket->pChunk.get() ) ) + 2;

			// todo: ��ȡ chunk-extension
		}

		// �жϵ�ǰ chunk �Ƿ��ȡ����
		if( pIncomingPacket->nChunkLen > 0 )
		{
			
		}
		else
		{
			// ��Ӧ�ý�������
			return -1;
		}
	}

	return 0;
}
		
int32_t	CHttpPacketLayer::ProcessHeader( tagHttpIncomingPacket *pIncomingPacket, u_int32_t *puAction )
{
	// ����Ƿ��ͷ��ȫ��
	char						*pstrDelimiterPos;
	std::string					strHeader, strKey, strValue;
	std::string::size_type		pos;
	CMapHttpHeader::iterator	itrHeader;

	if( pIncomingPacket == NULL || puAction == NULL )
	{
		return -1;
	}
	*puAction = 0;

	if( pIncomingPacket->pPacket.get() == NULL || pIncomingPacket->uPacketHttpLen < 4 ) // ���ٵð��� "\r\n\r\n"
	{
		return -1;
	}

	// ע��,����Ҫ�� pIncomingPacket->pPacket һ�������β
	pstrDelimiterPos = strstr( ( char* )( pIncomingPacket->pPacket.get() ), "\r\n\r\n" );
	if( pstrDelimiterPos == NULL )
	{
		// ��δ��ȫ��ͷ,������
		*puAction = 3;
		return 0;
	}

	// ��ͷ����
	pIncomingPacket->uHeadLen = ( unsigned int32_t )( ( pstrDelimiterPos + 4 ) - ( char* )( pIncomingPacket->pPacket.get() ) );
	strHeader.assign( ( char* )( pIncomingPacket->pPacket.get() ), pIncomingPacket->uHeadLen );

	std::cout << "---------header----------" << std::endl;
	std::cout << strHeader << std::endl;
	std::cout << "-------------------------" << std::endl;

	// ���� http ͷ
	boost::regex 	regexFirstLine( "(.+?)\\s+(.+?)\\s+(.+?)\\s*$", boost::regbase::icase );
	boost::regex 	regexParam( "\\s*(.+?)\\s*:\\s*(.+?)\\s*$", boost::regbase::icase );
	boost::smatch	matchesFirstLine, matchesParam;
	std::string::const_iterator itrBegin, itrEnd;
	
	// �Ƚ�����һ��
	pos = strHeader.find( "\r\n" );
	if( pos == std::string::npos )
	{
		// http ��ʽ����
		// todo: �����ص�
		*puAction = 2;
		return 0;
	}

	itrBegin = strHeader.begin();
	itrEnd = ( itrBegin + pos + 2 ); // +2 ��Ϊ�˰����� "\r\n",���ڵ� itrEnd ָ�� \r\n ��ĵ�һ���ַ�
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
		// http ��Ӧ��ʽ����
		*puAction = 2;
		return 0;
	}

	std::cout << "------header params------" << std::endl;

	// ��������ļ�ֵ��
	itrBegin = itrEnd;
	itrEnd = strHeader.end();
	while( boost::regex_search( itrBegin, itrEnd, matchesParam, regexParam ) )
	{
		strKey = matchesParam[ 1 ];
		strValue = matchesParam[ 2 ];

		// key תСд	
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
		// ���ʣ���ڴ��Ƿ񹻱��ν��ղ���
		if( pIncomingPacket->uPacketHttpLen + DEF_RECEIVING_BUFF_LEN > pIncomingPacket->uPacketBuffLen )
		{
			boost::shared_array< u_int8_t >	pNewBuff;
	
			pNewBuff = m_pMemPool->Malloc( pIncomingPacket->uMallocSize );
			if( pNewBuff.get() == NULL )
			{
				std::cerr << "outof_mem " << __FILE__ << ":" << __LINE__ << std::endl;
				return 0;
			}
	
			// �������ڴ����
			if( pIncomingPacket->pPacket.get() != NULL && pIncomingPacket->uPacketHttpLen != 0 )
			{
				memcpy( pNewBuff.get(), pIncomingPacket->pPacket.get(), pIncomingPacket->uPacketHttpLen );
			}
			
			pIncomingPacket->pPacket = pNewBuff;
			pIncomingPacket->uPacketBuffLen = pIncomingPacket->uMallocSize;
	
			// �´ν������������ڴ�
			pIncomingPacket->uMallocSize = pIncomingPacket->uMallocSize << 1;
		}
	}
	else
	{
		if( pIncomingPacket->nChunked == -1 )
		{
			// ��Ӧ�ý�������
			return -1;
		}
		else if( pIncomingPacket->nChunked == 0 )
		{
			// ���ʣ���ڴ��Ƿ񹻱��ν��ղ���
			if( pIncomingPacket->uPacketHttpLen + DEF_RECEIVING_BUFF_LEN > pIncomingPacket->uPacketBuffLen )
			{
				boost::shared_array< u_int8_t >	pNewBuff;
		
				pNewBuff = m_pMemPool->Malloc( pIncomingPacket->uMallocSize );
				if( pNewBuff.get() == NULL )
				{
					std::cerr << "outof_mem " << __FILE__ << ":" << __LINE__ << std::endl;
					return 0;
				}
		
				// �������ڴ����
				if( pIncomingPacket->pPacket.get() != NULL && pIncomingPacket->uPacketHttpLen != 0 )
				{
					memcpy( pNewBuff.get(), pIncomingPacket->pPacket.get(), pIncomingPacket->uPacketHttpLen );
				}
				
				pIncomingPacket->pPacket = pNewBuff;
				pIncomingPacket->uPacketBuffLen = pIncomingPacket->uMallocSize;
		
				// �´ν������������ڴ�
				pIncomingPacket->uMallocSize = pIncomingPacket->uMallocSize << 1;
			}
		}
		else // nChunked == 1
		{
			// ���ʣ���ڴ��Ƿ񹻱��ν��ղ���
			if( pIncomingPacket->uChunkHttpLen + DEF_RECEIVING_BUFF_LEN > pIncomingPacket->uChunkBuffLen )
			{
				boost::shared_array< u_int8_t >	pNewBuff;
		
				pNewBuff = m_pMemPool->Malloc( pIncomingPacket->uMallocSize );
				if( pNewBuff.get() == NULL )
				{
					std::cerr << "outof_mem " << __FILE__ << ":" << __LINE__ << std::endl;
					return 0;
				}
		
				// �������ڴ����
				if( pIncomingPacket->pChunk.get() != NULL && pIncomingPacket->uChunkHttpLen != 0 )
				{
					memcpy( pNewBuff.get(), pIncomingPacket->pChunk.get(), pIncomingPacket->uChunkHttpLen );
				}
				
				pIncomingPacket->pChunk = pNewBuff;
				pIncomingPacket->uChunkBuffLen = pIncomingPacket->uMallocSize;
		
				// �´ν������������ڴ�
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
				DEF_RECEIVING_BUFF_LEN - 1, // -1 ��Ϊ���������һ���ֽ���� \0
				0 );

		if( *pnReceivedLen > 0 )
		{
			pIncomingPacket->uPacketHttpLen += *pnReceivedLen;
			*( char* )( pIncomingPacket->pPacket.get() + pIncomingPacket->uPacketHttpLen ) = '\0'; // 0 ��β��,��Ϊ strstr ������Ҫ������� 0 ��β���ַ���
		}
	}
	else
	{
		if( pIncomingPacket->nChunked == -1 )
		{
			// ��Ӧ�ý�������
			return -1;
		}
		else if( pIncomingPacket->nChunked == 0 )
		{
			*pnReceivedLen = recv( fd, 
					pIncomingPacket->pPacket.get() + pIncomingPacket->uPacketHttpLen, 
					DEF_RECEIVING_BUFF_LEN - 1, // -1 ��Ϊ���������һ���ֽ���� \0
					0 );

			if( *pnReceivedLen > 0 )
			{
				pIncomingPacket->uPacketHttpLen += *pnReceivedLen;
				*( char* )( pIncomingPacket->pPacket.get() + pIncomingPacket->uPacketHttpLen ) = '\0'; // 0 ��β��,��Ϊ strstr ������Ҫ������� 0 ��β���ַ���
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


