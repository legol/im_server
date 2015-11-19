// author:chenjie

#ifndef _EPOLL_THREAD_INCLUDED_
#define _EPOLL_THREAD_INCLUDED_

#include <map>
#include <string>
#include <boost/smart_ptr.hpp>
#include "epollevent.h"
#include "tcppacket.h"
#include "thread_model.h"

class CEpollThread : public CThreadModel
{
	public:
		CEpollThread();
		virtual ~CEpollThread();

	public:
		virtual	int32_t	Create(); 
		virtual	int32_t Run( void *pArg );
		virtual int32_t Stop();
		virtual	int32_t	RunOnce();

	public:
		boost::shared_ptr< CEpollEvent >	m_pEpollEvent;
};

extern	boost::shared_ptr< CEpollThread >	g_pEpollThread;

#endif // _EPOLL_THREAD_INCLUDED_
