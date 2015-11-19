#include "epoll_thread.h"

boost::shared_ptr< CEpollThread >	g_pEpollThread;

CEpollThread::CEpollThread()
{
}

CEpollThread::~CEpollThread()
{
	Stop();
}

int32_t	CEpollThread::Create()
{
	if( m_pEpollEvent.get() != NULL )
	{
		// 已经调用过 create 函数，忽略重复的调用
		return 0;
	}

	m_pEpollEvent = boost::shared_ptr< CEpollEvent >( new (std::nothrow) CEpollEvent() );
	if( m_pEpollEvent.get() == NULL )
	{
		return -1;
	}

	if( m_pEpollEvent->Init( 1000000 ) != 0 )
	{
		return -1;
	}

	return CThreadModel::Create();
}

int32_t CEpollThread::Stop()
{
	bool	bIsRunning = true;

	IsRunning( &bIsRunning );
	if( bIsRunning == false )
	{
		// 已经停止了，忽略对 stop 的调用
		return 0;
	}

	if( m_pEpollEvent )
	{
		m_pEpollEvent->Stop(); 
	}

	return CThreadModel::Stop();
}

int32_t	CEpollThread::RunOnce()
{
	if( m_pEpollEvent )
	{
		return m_pEpollEvent->Run();
	}

	return -1;
}

int32_t CEpollThread::Run( void *pArg )
{
	bool	bIsRunning = false;

	IsRunning( &bIsRunning );
	if( bIsRunning )
	{
		// 已经在运行了，忽略重复调用
		return 0;
	}

	return CThreadModel::Run( pArg );
}


