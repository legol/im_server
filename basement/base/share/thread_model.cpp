#include "thread_model.h"

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>

void* ThreadMain_External( void* pArg )
{
	tagThreadArg 	*pThreadArg = ( tagThreadArg* )pArg;
	void			*pvRet;

	if( pThreadArg == NULL )
	{
		pthread_exit( NULL );
		return NULL;
	}

	if( pThreadArg->pThis == NULL )
	{
		pthread_exit( NULL );
		return NULL;
	}

	pvRet = pThreadArg->pThis->ThreadMain( pThreadArg->pArg );

	pthread_exit( pvRet );

	return pvRet;
}

CThreadModel::CThreadModel()
{
	pthread_mutex_init( &m_lockRunning, NULL );
	m_bRunning = false;

	sem_init( &m_sem, 0, 0 );
}

CThreadModel::~CThreadModel()
{
}

int32_t CThreadModel::Create()
{
	return 0;
}

int32_t	CThreadModel::FreeAll()
{
	Stop();

	return 0;
}

int32_t CThreadModel::Run( void *pArg )
{
	bool	bIsRunning = false;

	IsRunning( &bIsRunning );
	if( bIsRunning )
	{
		return -1;
	}

	pthread_mutex_lock( &m_lockRunning );
	m_bRunning = true;	
	pthread_mutex_unlock( &m_lockRunning );

	m_objThreadArg.pThis = this;
	m_objThreadArg.pArg = pArg;
	return pthread_create( &m_thread, NULL, ThreadMain_External, ( void* )( &m_objThreadArg ) );
}

int32_t	CThreadModel::Stop()
{
	bool bIsRunning;

	pthread_mutex_lock( &m_lockRunning );
	bIsRunning = m_bRunning;
	m_bRunning = false;
	pthread_mutex_unlock( &m_lockRunning );

	if( bIsRunning )
	{
		sem_destroy( &m_sem );
		pthread_join( m_thread, NULL );
	}

	return 0;
}

int32_t CThreadModel::Join()
{
	bool	bIsRunning;

	IsRunning( &bIsRunning );
	if( bIsRunning == false )
	{
		return 0;
	}

	return pthread_join( m_thread, NULL );
}

int32_t	CThreadModel::IsRunning( bool *pbRunning )
{
	if( pbRunning == NULL )	
	{
		return -1;
	}

	pthread_mutex_lock( &m_lockRunning );
	*pbRunning = m_bRunning;
	pthread_mutex_unlock( &m_lockRunning );

	return 0;

}

void* CThreadModel::ThreadMain( void* pvArg )
{
	bool		bIsRunning;
	int32_t		nRet = 0;

	if ( ( nRet = RunOnce() ) != 0 )
	{
		return ( void* )( int64_t )nRet;
	}

	while( true )
	{
		// 检测是否退出线程
		IsRunning( &bIsRunning );
		if( bIsRunning == false ) 
		{
			std::cout<< "Thread has exited; " << std::endl;
			break;
		}

		if ( ( nRet = RunTimes() ) != 0 )
		{
			std::cout << "RunTimes() != 0 " << std::endl;
			break;
		}
	}


	return ( void* )( int64_t )nRet;
}

int32_t	CThreadModel::RunOnce()
{
	return 0;
}

int32_t	CThreadModel::RunTimes()
{
	return 0;
}

