#ifndef _HANDLER_INCLUDED_
#define _HANDLER_INCLUDED_

#include <sys/types.h>
#include <boost/smart_ptr.hpp>
#include "httppacket.h"
#include "thread_pool.h"
#include "server_thread.h"
#include "client_thread.h"
#include "general_data.h"


// 所有异步的逻辑处理都在这里进行

enum EEventType
{
	EVENT_TYPE_UNKONWN 				= 0,
	EVENT_TYPE_PACKET_RECEIVED		= 1,
	EVENT_TYPE_PACKET_SENT			= 2,
	EVENT_TYPE_PACKET_TIMEOUT		= 3,
	EVENT_TYPE_CONNECTED			= 4,
	EVENT_TYPE_CLOSE				= 5,
};

struct tagHttpEvent
{
	std::string						strEventSource; // 来自那个服务
	EEventType						eEventType; // 事件的类型

	int32_t							fd;
	sockaddr_in						addr;

	boost::shared_array< u_int8_t >		pHttpHead;
	u_int32_t						uHttpHeadLen;

	boost::shared_array< u_int8_t > 		pHttpBody; 
	u_int32_t						uHttpBodyLen;

	u_int32_t						uPacketId;

	boost::shared_ptr< CHttpPacketLayer >	pPacketLayer;

	tagHttpEvent()
	{
		eEventType = EVENT_TYPE_UNKONWN;

		fd = -1;
		memset( &addr, 0, sizeof( sockaddr_in ) );
		uHttpHeadLen = 0;
		uHttpBodyLen = 0;
		uPacketId = 0;
	}
};

class CHandler : public IThreadPoolCallback< tagHttpEvent >
{
	public:
		CHandler();
		virtual ~CHandler();

	public:
		virtual	int32_t	Init();
		virtual	int32_t	Process( tagHttpEvent item );

	protected:
};

#endif // _HANDLER_INCLUDED_
