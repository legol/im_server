#ifndef _TEST_MAIN_INCLUDED_
#define _TEST_MAIN_INCLUDED_

#include <sys/types.h>
#include <boost/smart_ptr.hpp>
#include "mem_pool.h"
#include "epollevent.h"
#include "timeoutmgr.h"
#include "httppacket.h"
#include "thread_pool.h"
#include "client_thread.h"
#include "general_data.h"
	
extern  boost::shared_ptr< CMemPool >			g_pMemPool;

extern boost::shared_ptr< CClientThread >		g_pClientThread_HttpTest;
extern boost::shared_ptr< CGeneralData >		g_pConfig;
extern CLocalHost						g_objLocalHost;

#endif // _TEST_MAIN_INCLUDED_
