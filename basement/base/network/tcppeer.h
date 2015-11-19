#ifndef _TCP_PEER_INCLUDED_
#define _TCP_PEER_INCLUDED_

#include <sys/types.h>
#include <netinet/in.h>
#include <map>
#include <string.h>
#include <boost/thread/mutex.hpp>
#include "cppsink.h"

class CPacketId
{
	public:
		CPacketId();
		virtual ~CPacketId();
		virtual int32_t GeneratePacketId( u_int32_t *puPacketId );

	protected:
		boost::mutex		m_lockPacketId;
		u_int32_t	m_uPacketId;

};


/////////////////////////////////////////////////////////////////////////////////////////////
struct tagSocketStatus
{
	bool      	bConnected; // 这个 fd 是否已经连接
	bool      	bCanWrite; // 这个 fd 是否可写

	sockaddr_in	addr;

	tagSocketStatus()
	{
		bConnected = false;
		bCanWrite = true;
		memset( &addr, 0, sizeof( addr ) );
	}
};
typedef std::map< int32_t, tagSocketStatus >  CMapSocketStatus; // fd--这个fd的状态


class ITcpEventCallback
{
	public:
		ITcpEventCallback(){ }
		virtual ~ITcpEventCallback(){}

		virtual int32_t OnConnected        ( int32_t fd, sockaddr_in addr, void *pThis ) { return -1; }
		virtual int32_t OnConnectionRefused( int32_t fd, void *pThis ) { return -1; }
		virtual int32_t OnDataIn           ( int32_t fd, void *pThis ) { return -1; }
		virtual int32_t OnDataOut          ( int32_t fd, void *pThis ) { return -1; }
		virtual int32_t OnNewSocket        ( int32_t fd, void *pThis ) { return -1; }
		virtual int32_t OnClose            ( int32_t fd, void *pThis ) { return -1; }
		virtual int32_t OnError            ( int32_t fd, void *pThis ) { return -1; }
		virtual int32_t OnSendPacket       ( int32_t fd, void *pThis ) { return -1; }
		virtual void 	OnStart       	   ( void *pThis ) { }
	public:
};

/////////////////////////////////////////////////////////////////////////////////////////////
CPP_SINK_BEGIN( CTcpEventCallback ) 
	CPP_SINK_FUNC( int32_t, OnConnected, (int32_t fd, sockaddr_in addr, void *pThis), ( fd, addr, pThis ) )
	CPP_SINK_FUNC( int32_t, OnConnectionRefused, (int32_t fd, void *pThis), ( fd, pThis ) )
	CPP_SINK_FUNC( int32_t, OnDataIn, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( int32_t, OnDataOut, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( int32_t, OnNewSocket, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( int32_t, OnClose, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( int32_t, OnError, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( int32_t, OnSendPacket, ( int32_t fd, void *pThis ), ( fd, pThis ) )
	CPP_SINK_FUNC( void, OnStart, ( void *pThis ), ( pThis ) )
CPP_SINK_END()
DECLARE_CPP_SINK_OBJ( CTcpEventCallback, ITcpEventCallback )


#endif // _TCP_PEER_INCLUDED_
