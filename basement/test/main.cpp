#include <stdio.h>

#include <sys/types.h>
#include <boost/smart_ptr.hpp>
#include <sstream>

#include "mem_pool.h"
#include "epollevent.h"
#include "timeoutmgr.h"
#include "thread_pool.h"
#include "client_thread.h"
#include "general_data.h"
#include "client_cb.h"
#include "epoll_thread.h"
#include "main.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
boost::shared_ptr< CMemPool >				g_pMemPool;
boost::shared_ptr< CTimer >				g_pTimer;
boost::shared_ptr< CTimeoutMgr >			g_pTimeoutMgr;
boost::shared_ptr< CPacketId >			g_pPacketId;

boost::shared_ptr< CHandler >						g_pHandler; // 线程池的回调

boost::shared_ptr< CGeneralData >			g_pConfig;

CLocalHost							g_objLocalHost;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t read_conf( )
{
    if ( g_pConfig.get() != NULL )
    {
        g_pConfig.reset();
    }
    g_pConfig = CGeneralData::CreateInstance();

//    // 内存池
//    if ( conf.Exist("mem_pool") == false )
//    {
//        std::cout << "missing mem_pool" << std::endl;
//        return -1;
//    }
//    g_pConfig->SetStr( "mem_pool", conf.GetValue<std::string>("mem_pool") );
//
//    // 工作者线程的个数
//    if ( conf.Exist("worker_count") == false )
//    {
//        std::cout << "missing worker_count" << std::endl;
//        return -1;
//    }
//    g_pConfig->SetUint32( "worker_count", conf.GetValue< u_int32_t >("worker_count") );

	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t init()
{
    std::string strMemPoolCfg = "(512,20000),"
                                "(1024,10000),(2048,10000),(4096,1000),(8192,1000),"
                                "(16384,1000),(32768,1000),(65536,100),"
                                "(131072,100),(262144,100),(524288,100),"
                                "(1048576,100),(2097152,10),(4194304,10)";
    u_int32_t   uWorkerCount = 2;

    if( g_pConfig.get() == NULL )
    {
        return -1;
    }

//    if( g_pConfig->GetStr( "mem_pool", &strMemPoolCfg ) != 0 )
//    {
//        return -1;
//    }

//    if( g_pConfig->GetUint32( "worker_count", &uWorkerCount ) != 0 )
//    {
//        return -1;
//    }

    // 内存池初始化
    g_pMemPool = boost::shared_ptr< CMemPool >( new (std::nothrow) CMemPool() );
    if( g_pMemPool.get() == NULL )
    {
        return -1;
    }
    g_pMemPool->Init( strMemPoolCfg );

    // 几个网络公用组件初始化
    g_pTimer = boost::shared_ptr< CTimer >( new (std::nothrow) CTimer() );
    if( g_pTimer.get() == NULL )
    {
        return -1;
    }
    g_pTimer->StartTimer( 3600 * 1000 );

    g_pTimeoutMgr = boost::shared_ptr< CTimeoutMgr >( new (std::nothrow) CTimeoutMgr() );
    if( g_pTimeoutMgr.get() == NULL )
    {
        return -1;
    }
    g_pTimeoutMgr->Init( g_pTimer );

    g_pPacketId = boost::shared_ptr< CPacketId >( new (std::nothrow) CPacketId() );
    if( g_pPacketId.get() == NULL )
    {
        return -1;
    }

	// 创建 epoll 线程
	g_pEpollThread = boost::shared_ptr< CEpollThread >( new (std::nothrow) CEpollThread() );
	if( g_pEpollThread.get() == NULL )
	{
	    return -1;
	}
	
	if( g_pEpollThread->Create() != 0 )
	{
	    return -1;
	}

    // 线程池初始化
//    g_pThreadPool = boost::shared_ptr< CThreadPool< tagHttpEvent > >( new (std::nothrow) CThreadPool< tagHttpEvent >() );
//    if( g_pThreadPool.get() == NULL )
//    {
//        return -1;
//    }
//    g_pThreadPool->Init( uWorkerCount );
//    g_pThreadPool->Run( NULL ); // 启动线程池中的若干线程

    // 线程池回调初始化
    g_pHandler = boost::shared_ptr< CHandler >( new (std::nothrow) CHandler() );
    if( g_pHandler.get() == NULL )
    {
        return -1;
    }
    g_pHandler->Init();

    return 0;
}


int main()
{
	if( read_conf() != 0 )
	{
        std::cerr << "failed to read_conf" << std::endl;
        return -1;
	}

	if( init() != 0 )
    {
        std::cerr << "failed to init" << std::endl;
        return -1;
    }

	std::stringstream	ss;
	std::string			strRequest;

	ss << "GET / HTTP/1.1\r\n";
	ss << "\r\n";

	strRequest = ss.str();

	boost::shared_array< u_int8_t > pTmp = g_pMemPool->Malloc( strRequest.length() );
	memcpy( pTmp.get(), strRequest.c_str(), strRequest.length() );

	// 启动网络线程
	g_pEpollThread->Run( NULL );

	// 如果要结束，需要显示的停止 timer 线程和 timerdisp线程，否则会死锁
	g_pEpollThread->Join();

	return 0;
}
