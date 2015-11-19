#include "tcppacket.h" 
#include <sys/types.h>


CPacketLayer::CPacketLayer()
{
	m_pCallback_ITcpEventCallback = boost::shared_ptr< CTcpEventCallbackObj< CPacketLayer > >( new CTcpEventCallbackObj< CPacketLayer >( this ) );
	m_pCallback_ITcpEventCallback->Hook_OnConnected( &CPacketLayer::OnConnected );
	m_pCallback_ITcpEventCallback->Hook_OnConnectionRefused( &CPacketLayer::OnConnectionRefused );
	m_pCallback_ITcpEventCallback->Hook_OnDataIn( &CPacketLayer::OnDataIn );
	m_pCallback_ITcpEventCallback->Hook_OnDataOut( &CPacketLayer::OnDataOut );
	m_pCallback_ITcpEventCallback->Hook_OnNewSocket( &CPacketLayer::OnNewSocket );
	m_pCallback_ITcpEventCallback->Hook_OnClose( &CPacketLayer::OnClose );
	m_pCallback_ITcpEventCallback->Hook_OnError( &CPacketLayer::OnError );
	m_pCallback_ITcpEventCallback->Hook_OnSendPacket( &CPacketLayer::OnSendPacket );
	m_pCallback_ITcpEventCallback->Hook_OnStart( &CPacketLayer::OnStart );

	m_bIsClient = true;
}

CPacketLayer::~CPacketLayer()
{
	m_pCallback_ITcpEventCallback->UnhookAll();
}

int32_t	CPacketLayer::SendPacket( int32_t fd, boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, bool bNeedResp, u_int32_t *puPacketId, void* pvParam )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockSend );

	CMapSending::iterator   itrSending;
	tagOutgoingPacket       objPacket;

	u_int32_t                uPacketId = 0;

	if( ( NULL == pchPacket.get() )
			||(  0   == uPacketLen )
			||( NULL == puPacketId) )
	{
		return -1;
	}

	itrSending = m_mapSending.find( fd );
	if( itrSending == m_mapSending.end() )
	{
		m_mapSending[ fd ] = CLstSending();
		itrSending = m_mapSending.find( fd );
	}

	objPacket.pPacket = pchPacket;
	objPacket.uPacketLen = uPacketLen;
	objPacket.uPacketSent = 0;
	objPacket.bNeedResp = bNeedResp;

	if( m_bIsClient )
	{
		if( m_pPacketId )
		{
			m_pPacketId->GeneratePacketId( &uPacketId );
		}
		*puPacketId = uPacketId;
	}
	else
	{
		uPacketId = *puPacketId;
	}

	if( m_pPacketCallback )
	{
		if( m_pPacketCallback->OnPreSendPacket( fd, objPacket.pPacket, objPacket.uPacketLen, uPacketId, pvParam ) != 0 )
		{
			return -1;
		}
	}

	itrSending->second.push_back( objPacket );

	return 0;
}

int32_t	CPacketLayer::OnConnected(int32_t fd, sockaddr_in addr, void *pThis)
{
	boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );

	CMapSocketStatus::iterator	itr;

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

	return -1; 
}

int32_t	CPacketLayer::OnConnectionRefused(int32_t fd, void *pThis)
{ 

	CMapSocketStatus::iterator	itr;
	sockaddr_in									addr;

	{
		boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );
		itr = m_mapSocketStatus.find( fd );
		if( itr == m_mapSocketStatus.end() )
		{
			std::cerr << "error: itr == m_mapSocketStatus.end()" << std::endl;
			return -1;
		}

		addr = itr->second.addr;

		m_mapSocketStatus.erase( fd );
		m_mapReceiving.erase( fd );
	}

	{
		boost::recursive_mutex::scoped_lock lock( m_lockSend );
		m_mapSending.erase( fd );
	}

	if( m_pPacketCallback )
	{
		return m_pPacketCallback->OnConnectionRefused( fd, addr );
	}

	return -1; 
}

int32_t	CPacketLayer::OnDataIn(int32_t fd, void *pThis)
{ 
	boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );

	CMapSocketStatus::iterator	itr;
	sockaddr_in					addr;

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

	if( NULL == m_pPacketCallback.get() )
	{
		std::cerr << "error: packet callback null" << std::endl;
		return -1;
	}

	itr->second.bConnected = true;
	addr = itr->second.addr;

	CMapReceiving::iterator	itrReceiving;
	tagIncomingPacket		objPacket;
	int32_t					nReceivedLen = 0;

	while( true ) // 一直收到 EAGAIN，或者出错
	{
		itrReceiving = m_mapReceiving.find( fd );
		if( itrReceiving != m_mapReceiving.end() )
		{
			// 有包没收完
			if( itrReceiving->second.uPacketHeadReceived != itrReceiving->second.uPacketHeadLen )
			{
				if( itrReceiving->second.uPacketHeadReceived > itrReceiving->second.uPacketHeadLen )
				{
					return -1; // 不应该进入这里
				}

				// 包头没收完
				nReceivedLen = recv( fd, 
						itrReceiving->second.pPacketHead.get() + itrReceiving->second.uPacketHeadReceived, 
						itrReceiving->second.uPacketHeadLen - itrReceiving->second.uPacketHeadReceived,
						0 );

				if( 0 == nReceivedLen )
				{
					// 连接被对方断开
					return -1;
				}
				else if( -1 == nReceivedLen )
				{
					if( (EAGAIN == errno) || (EINTR == errno) )
					{
						// 下次再收
						return 0;
					}
					else
					{
						// 出错了
						return -1;
					}
				}
				else if ( nReceivedLen == ( int32_t )( itrReceiving->second.uPacketHeadLen - itrReceiving->second.uPacketHeadReceived ) )
				{
					// 包头收全了
					itrReceiving->second.uPacketHeadReceived = itrReceiving->second.uPacketHeadLen;

					// 获取包体的大小
					m_pPacketCallback->OnGetPacketBodyLen( fd, addr, itrReceiving->second.pPacketHead, itrReceiving->second.uPacketHeadLen, &( itrReceiving->second.uPacketBodyLen ) );
					if( itrReceiving->second.uPacketBodyLen == 0 ) // 没有包体的
					{
						// 整个包收全了
						// 发出回调
						boost::shared_array< uint8_t >	pPacket;

						// 只需拷贝包头进去即可,包体已经在里面了
						itrReceiving->second.pPacket = m_pMemPool->Malloc( itrReceiving->second.uPacketHeadLen );
						pPacket = itrReceiving->second.pPacket;
						memcpy( pPacket.get(), itrReceiving->second.pPacketHead.get(), itrReceiving->second.uPacketHeadLen );

						// 如果在超时队列中有对应项，即还未超时，则对逻辑层发出回调 
						if( m_pPacketCallback )
						{
							int32_t			nRet;
							u_int32_t		uPacketId;
							bool			bPacketExist = true;

							nRet = m_pPacketCallback->OnGetPacketId( fd, addr, pPacket, itrReceiving->second.uPacketHeadLen + itrReceiving->second.uPacketBodyLen, false, &uPacketId );
							if( nRet != 0 )
							{
								return nRet;
							}

							if( m_pTimeoutMgr )
							{
								if (m_bIsClient)
								{
									boost::recursive_mutex::scoped_lock	lock( m_pTimeoutMgr->m_lockTimeoutMgr );

									m_pTimeoutMgr->IsPacketExist( uPacketId, &bPacketExist );

									if( bPacketExist )
									{
										m_pTimeoutMgr->DelPacket( uPacketId );
									}
								}

								if (bPacketExist)
								{
									nRet = m_pPacketCallback->OnPacketReceived( fd, addr, pPacket, itrReceiving->second.uPacketHeadLen + itrReceiving->second.uPacketBodyLen, uPacketId );
									if( nRet != 0 )
									{
										return nRet;
									}
								}
							}
							else
							{
								nRet = m_pPacketCallback->OnPacketReceived( fd, addr, pPacket, itrReceiving->second.uPacketHeadLen + itrReceiving->second.uPacketBodyLen, uPacketId );
								if( nRet != 0 )
								{
									return nRet;
								}
							}
						}

						// 开始收下一个包
						// 获取包头的大小
						objPacket.uPacketHeadReceived = 0;
						objPacket.uPacketHeadLen = 0;
						objPacket.uPacketBodyReceived = 0;
						objPacket.uPacketBodyLen = 0;
						objPacket.pPacketHead.reset();
						objPacket.pPacketBody = NULL;
						objPacket.pPacket.reset();
						m_pPacketCallback->OnGetPacketHeaderLen( fd, addr, &( objPacket.uPacketHeadLen ) );
						objPacket.pPacketHead = m_pMemPool->Malloc( objPacket.uPacketHeadLen );
						if( objPacket.pPacketHead.get() == NULL )
						{
							// 内存不足
							std::cout << "not enough mem: " << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ << ")" << std::endl;
							return -1;
						}

						m_mapReceiving[ fd ] = objPacket;

						continue; // 继续收
					}

					itrReceiving->second.pPacket = m_pMemPool->Malloc( itrReceiving->second.uPacketHeadLen + itrReceiving->second.uPacketBodyLen );
					if (itrReceiving->second.pPacket.get() == NULL)
					{
						std::cout << "not enough mem, len="<< itrReceiving->second.uPacketBodyLen << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ << ")" << std::endl;						
						return -1;
					}
					itrReceiving->second.pPacketBody = itrReceiving->second.pPacket.get() + itrReceiving->second.uPacketHeadLen;

					continue; // 继续收
				}
				else if ( nReceivedLen < (int32_t)( itrReceiving->second.uPacketHeadLen - itrReceiving->second.uPacketHeadReceived ) )
				{
					// 继续收
					itrReceiving->second.uPacketHeadReceived += nReceivedLen;
					continue;
				}
				else
				{
					// 不应该进入这里
					return -1;
				}
			}
			else if( itrReceiving->second.uPacketBodyReceived != itrReceiving->second.uPacketBodyLen )
			{
				// 包体没收完
				nReceivedLen = recv( fd, 
						itrReceiving->second.pPacketBody + itrReceiving->second.uPacketBodyReceived, 
						itrReceiving->second.uPacketBodyLen - itrReceiving->second.uPacketBodyReceived,
						0 );

				if( 0 == nReceivedLen )
				{
					// 连接被对方断开
					return -1;
				}
				else if( -1 == nReceivedLen )
				{
					if( (EAGAIN == errno) || (EINTR == errno) )
					{
						// 下次再收
						return 0;
					}
					else
					{
						// 出错了
						return -1;
					}
				}
				else if ( nReceivedLen == ( int32_t )( itrReceiving->second.uPacketBodyLen - itrReceiving->second.uPacketBodyReceived ) )
				{
					// 包体收全了
					itrReceiving->second.uPacketBodyReceived = itrReceiving->second.uPacketBodyLen;

					// 发出回调
					boost::shared_array< uint8_t >	pPacket;

					// 只需要把包头拷贝进去即可,包体已经在里面了
					pPacket = itrReceiving->second.pPacket;
					memcpy( pPacket.get(), itrReceiving->second.pPacketHead.get(), itrReceiving->second.uPacketHeadLen );

					// 清理超时队列中的对应项

					// 如果在超时队列中有对应项，即还未超时，则对逻辑层发出回调 
					if( m_pPacketCallback )
					{
						int32_t			nRet;
						u_int32_t		uPacketId;
						bool            bPacketExist = true;

						nRet = m_pPacketCallback->OnGetPacketId( fd, addr, pPacket, itrReceiving->second.uPacketHeadLen + itrReceiving->second.uPacketBodyLen, false, &uPacketId );
						if( nRet != 0 )
						{
							return nRet;
						}

						if( m_pTimeoutMgr )
						{
							do // 需要同步，是因为在获得了 bPacketExist 值之后，如果发生线程切换，那么这个值将会错误
							{
								if( m_bIsClient )
								{
									boost::recursive_mutex::scoped_lock	lock( m_pTimeoutMgr->m_lockTimeoutMgr );                            

									m_pTimeoutMgr->IsPacketExist( uPacketId, &bPacketExist );

									if( bPacketExist )
									{
										m_pTimeoutMgr->DelPacket( uPacketId );
									}
								}

								if( bPacketExist )
								{
									nRet = m_pPacketCallback->OnPacketReceived( fd, addr, pPacket, itrReceiving->second.uPacketHeadLen + itrReceiving->second.uPacketBodyLen, uPacketId );
									if( nRet != 0 )
									{
										return nRet;
									}
								}
							}while( 0 );
						}
						else
						{
							nRet = m_pPacketCallback->OnPacketReceived( fd, addr, pPacket, itrReceiving->second.uPacketHeadLen + itrReceiving->second.uPacketBodyLen, uPacketId );
							if( nRet != 0 )
							{
								return nRet;
							}
						}
					}

					// 开始收下一个包
					// 获取包头的大小

					objPacket.uPacketHeadReceived = 0;
					objPacket.uPacketHeadLen = 0;
					objPacket.uPacketBodyReceived = 0;
					objPacket.uPacketBodyLen = 0;
					objPacket.pPacketHead.reset();
					objPacket.pPacketBody = NULL;
					objPacket.pPacket.reset();
					m_pPacketCallback->OnGetPacketHeaderLen( fd, addr, &( objPacket.uPacketHeadLen ) );
					objPacket.pPacketHead = m_pMemPool->Malloc( objPacket.uPacketHeadLen );
					if( NULL == objPacket.pPacketHead.get() )
					{
						std::cout << "not enough mem: " << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ << ")" << std::endl;						
						return -1;
					}

					m_mapReceiving[ fd ] = objPacket;

					continue; // 继续收
				}
				else if ( nReceivedLen < ( int32_t )( itrReceiving->second.uPacketBodyLen - itrReceiving->second.uPacketBodyReceived ) )
				{
					// 继续收
					itrReceiving->second.uPacketBodyReceived += nReceivedLen;
					continue;
				}
				else
				{
					// 不应该进入这里
					return -1;
				}
			}
			else
			{
				// 不应该进入这里
				return -1;
			}
		}
		else
		{
			// 没有收了一半的包，开始收一个新包
			objPacket.uPacketHeadLen = 0;
			objPacket.uPacketHeadReceived = 0;
			objPacket.uPacketBodyLen = 0;
			objPacket.uPacketBodyReceived = 0;
			objPacket.pPacketHead.reset();
			objPacket.pPacketBody = NULL;
			objPacket.pPacket.reset();
			m_pPacketCallback->OnGetPacketHeaderLen( fd, addr, &( objPacket.uPacketHeadLen ) );
			objPacket.pPacketHead = m_pMemPool->Malloc( objPacket.uPacketHeadLen );
			if( NULL == objPacket.pPacketHead.get() )
			{
				std::cout << "not enough mem: " << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ << ")" << std::endl;				
				return -1;
			}

			m_mapReceiving[ fd ] = objPacket;

			continue; // 开始收包
		}
	}

	return 0;
}

int32_t	CPacketLayer::OnDataOut(int32_t fd, void *pThis)
{ 
	boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );

	CMapSocketStatus::iterator	itrStatus;

	itrStatus = m_mapSocketStatus.find( fd );
	if( itrStatus == m_mapSocketStatus.end() )
	{
//		does this mean the fd is being closed?
//		tagSocketStatus	objSocketStatus;
//
//		objSocketStatus.bConnected = true;
//		objSocketStatus.bCanWrite = true;
//		memset( &( objSocketStatus.addr ), 0, sizeof( objSocketStatus.addr ) );
//		m_mapSocketStatus[ fd ] = objSocketStatus;
	}
	else
	{
		itrStatus->second.bConnected = true;
		itrStatus->second.bCanWrite = true;
	}

	return 0;
}

int32_t	CPacketLayer::OnSendPacket( int32_t fd, void *pThis )
{
	CMapSending::iterator				itrMap;
	CMapSocketStatus::iterator	        itrStatus;

	sockaddr_in                         addr;
	int32_t								nSent;
	CLstSending::iterator				itrLst;

	{	//缩小锁范围
		boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );
		itrStatus = m_mapSocketStatus.find( fd );
		if( itrStatus == m_mapSocketStatus.end() )
		{
			return -1;
		}

		addr = itrStatus->second.addr;
	}

	boost::recursive_mutex::scoped_lock lock( m_lockSend );

	itrMap = m_mapSending.find( fd );
	if( itrMap == m_mapSending.end()  )
	{
		// 没有要发送的东西
		return 0;
	}

	if( itrStatus->second.bConnected == false || ( itrStatus->second.bConnected == true && itrStatus->second.bCanWrite == false ) )
	{
		// 还未建立连接
		return 0;
	}

	itrLst = itrMap->second.begin();
	while( itrLst != itrMap->second.end() ) // 一直发送，直到这个 fd 下的全部发完，或者 EAGAIN
	{
		nSent = send( fd, (*itrLst).pPacket.get() + (*itrLst).uPacketSent, (*itrLst).uPacketLen - (*itrLst).uPacketSent, 0  );
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
				m_mapSending.erase( itrMap );
				return -1;
			}
		}
		else
		{
			if( ( u_int32_t )nSent == (*itrLst).uPacketLen - (*itrLst).uPacketSent )
			{
				// 一个逻辑包完整的发送出去了
				if( m_pPacketCallback )
				{
					int32_t			nRet;
					u_int32_t		uPacketId;

					// 获取 packet_id
					nRet = m_pPacketCallback->OnGetPacketId( fd, addr, (*itrLst).pPacket, (*itrLst).uPacketLen, true, &uPacketId );
					if( nRet != 0 )
					{
						return nRet;
					}

					if( ( *itrLst ).bNeedResp )
					{
						// 加入超时队列
						if( m_pTimeoutMgr )
						{
							m_pTimeoutMgr->AddPacket( ( *itrLst ).pPacket, ( *itrLst ).uPacketLen, uPacketId, m_uTimeoutInterval, m_pPacketCallback );
						}
					}

					// 发出回调
					nRet = m_pPacketCallback->OnPacketSent( fd, addr, (*itrLst).pPacket, (*itrLst).uPacketLen, uPacketId );
					if( nRet != 0 )
					{
						return nRet;
					}

				}

				itrMap->second.pop_front();
				itrLst = itrMap->second.begin();
				continue; // 继续发送下一个包
			}
			else if ( ( u_int32_t )nSent < (*itrLst).uPacketLen - (*itrLst).uPacketSent )
			{
				(*itrLst).uPacketSent += nSent;
				continue; // 继续发当前包
			}
			else
			{
				// 不应该进入这里
				m_mapSending.erase( itrMap );
				return -1;
			}
		}
	}

	return 0;
}

int32_t	CPacketLayer::OnNewSocket(int32_t fd, void *pThis)
{
	boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );

	tagSocketStatus	objSocketStatus;

	objSocketStatus.bConnected = false;
	objSocketStatus.bCanWrite = true;
	memset( &( objSocketStatus.addr ), 0, sizeof( objSocketStatus.addr ) );
	m_mapSocketStatus[ fd ] = objSocketStatus;

	return -1;
}

int32_t	CPacketLayer::OnClose(int32_t fd, void *pThis) 
{

	CMapSocketStatus::iterator	itr;
	sockaddr_in					addr;

	{
		boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );
		itr = m_mapSocketStatus.find( fd );
		if( itr == m_mapSocketStatus.end() )
		{
			return -1;
		}

		addr = itr->second.addr;

		m_mapSocketStatus.erase( fd );
		m_mapReceiving.erase( fd );
	}

	{
		boost::recursive_mutex::scoped_lock lock( m_lockSend );
		m_mapSending.erase( fd );
	}

	if( m_pPacketCallback )
	{
		return m_pPacketCallback->OnClose( fd, addr );
	}

	return -1; 
}

int32_t	CPacketLayer::OnError(int32_t fd, void *pThis)
{ 

	CMapSocketStatus::iterator  itr;
	sockaddr_in                 addr;

	{
		boost::recursive_mutex::scoped_lock lock( m_lockPacketLayer );
		itr = m_mapSocketStatus.find( fd );
		if( itr == m_mapSocketStatus.end() )
		{
			return -1;
		}

		addr = itr->second.addr;

		m_mapSocketStatus.erase( fd );
		m_mapReceiving.erase( fd );
	}

	{
		boost::recursive_mutex::scoped_lock lock( m_lockSend );
		m_mapSending.erase( fd );
	}

	if( m_pPacketCallback )
	{
		return m_pPacketCallback->OnError( fd, addr );
	}

	return -1; 
}

int32_t	CPacketLayer::Init( boost::shared_ptr< IPacketCallback > pPacketCb, 
		boost::shared_ptr< CMemPool > pMemPool, 
		boost::shared_ptr< CPacketId > pPacketId, 
		boost::shared_ptr< CTimeoutMgr > pTimeoutMgr, u_int32_t uTimeoutInterval )
{
	// comment by chenjie: new these components inside is a very bad idea ,because these components are designed to be unique.
	if( pPacketCb.get() == NULL || pMemPool.get() == NULL || pPacketId.get() == NULL || pTimeoutMgr.get() == NULL )
	{
		return -1;
	}

	m_pPacketCallback = pPacketCb;

	m_pMemPool = pMemPool;
	m_pPacketId = pPacketId;
	m_pTimeoutMgr = pTimeoutMgr;
	m_uTimeoutInterval = uTimeoutInterval;
	return 0;
}

void CPacketLayer::OnStart( void *pThis )
{
	if( m_pPacketCallback )
	{
		m_pPacketCallback->OnStart(  );
	}
}

boost::shared_ptr< ITcpEventCallback >	CPacketLayer::GetTcpEventCallback()
{
	boost::shared_ptr< ITcpEventCallback >	pRet;

	pRet = m_pCallback_ITcpEventCallback;
	return pRet;
}

