// author:chenjie
#ifndef _THREAD_POOL_INCLUDED_
#define _THREAD_POOL_INCLUDED_

#include <sys/types.h>
#include <pthread.h>
#include <memory>
#include <deque>
#include <iostream>
#include <semaphore.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/smart_ptr.hpp>
#include "thread_model.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template< class TWorkItemBase >
class CThreadPool;

enum EWorkItemState
{
	WORK_ITEM_STATE_START_PROCESS,
	WORK_ITEM_STATE_END_PROCESS
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 由 thread_pool 或者 worker thread 发出回调
template< class TWorkItemBase >
class IThreadPoolCallback
{
	public:
		IThreadPoolCallback(){};
		virtual ~IThreadPoolCallback(){};

		virtual	int32_t	OnStateChange( EWorkItemState eState, boost::shared_ptr< TWorkItemBase > pItem, int32_t nRet ){ return 0; };
		virtual	int32_t	Process( boost::shared_ptr< TWorkItemBase > pItem ){ return 0; };
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template< class TWorkItemBase >
struct tagWorkItem
{
	boost::shared_ptr< TWorkItemBase >							pItem;
	boost::shared_ptr< IThreadPoolCallback< TWorkItemBase >	> 	pThreadPoolCallback;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template< class TWorkItemBase >
class CWorkerThread : public CThreadModel
{
	public:
		CWorkerThread(){};
		virtual ~CWorkerThread(){};

		virtual	int32_t	Init( CThreadPool< TWorkItemBase > *pThreadPool )
		{
			if( pThreadPool == NULL )
			{
				return -1;
			}

			m_pThreadPool = pThreadPool;

			return 0;
		};

		virtual	int32_t	RunOnce()
		{
			return 0;
		};

		virtual	int32_t	RunTimes()
		{
			tagWorkItem< TWorkItemBase >	objWorkItem;
			bool						bQuit;

			if( m_pThreadPool )
			{
				m_pThreadPool->CheckQuit( &bQuit ); // 检查是否需要退出
				if( bQuit )
				{
					return 1;
				}

				m_pThreadPool->PopWorkItem( &objWorkItem );

				if( objWorkItem.pThreadPoolCallback.get() != NULL )
				{
					objWorkItem.pThreadPoolCallback->OnStateChange( WORK_ITEM_STATE_START_PROCESS, objWorkItem.pItem, 0 );
					int32_t nRet = objWorkItem.pThreadPoolCallback->Process( objWorkItem.pItem );
					objWorkItem.pThreadPoolCallback->OnStateChange( WORK_ITEM_STATE_END_PROCESS, objWorkItem.pItem, nRet );
				}
			}

			return 0;
		};

	protected:
		CThreadPool< TWorkItemBase >		*m_pThreadPool;

};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template< class TWorkItemBase >
class CThreadPool
{
	public:

	public:
		CThreadPool()
		{
			m_uMaxQueueLen = 100000;
			m_uThreadCount = 1;
			m_bQuit = false;
		};

		virtual ~CThreadPool()
		{
			if( m_pThread )
			{
				delete []m_pThread;
				m_pThread = NULL;
			}

			sem_destroy( &m_sem );
		};

		virtual	int32_t	Init( u_int32_t uThreadCount )
		{
			if( uThreadCount == 0 )
			{
				return -1;
			}

			m_uThreadCount = uThreadCount;
			m_pThread = new (std::nothrow) CWorkerThread< TWorkItemBase >[ m_uThreadCount ];
			if( m_pThread == NULL )
			{
				return -1;
			}

			for( int32_t nIdx = 0; nIdx < ( int32_t )m_uThreadCount; nIdx++ )
			{
				(m_pThread + nIdx )->Init( this );
			}

			sem_init( &m_sem, 0, 0 );
			m_bQuit = false;

			return 0;
		};

		virtual	int32_t Run( void* pvArg )
		{
			if( m_pThread == NULL )
			{
				return -1;
			}

			for( int32_t nIdx = 0; nIdx < ( int32_t )m_uThreadCount; nIdx++ )
			{
				( m_pThread + nIdx )->Run( pvArg );
			}

			return 0;

		};

		virtual int32_t	Join()
		{
			if( m_pThread == NULL )
			{
				return -1;
			}

			for( int32_t nIdx = 0; nIdx < ( int32_t )m_uThreadCount; nIdx++ )
			{
				( m_pThread + nIdx )->Join();
			}

			return 0;
		}

		virtual	int32_t	Stop()
		{
			if( m_pThread == NULL )
			{
				return -1;
			}

			for( int32_t nIdx = 0; nIdx < ( int32_t )m_uThreadCount; nIdx++ )
			{
				( m_pThread + nIdx )->Stop();
			}

			return 0;
		}

		// 安全退出
		virtual int32_t Quit()
		{
			boost::recursive_mutex::scoped_lock	lock( m_lockQuit );
			m_bQuit = true;

			return 0;
		}

		virtual	int32_t	CheckQuit( bool *pbQuit )
		{
			boost::recursive_mutex::scoped_lock	lock( m_lockQuit );

			if( pbQuit == NULL )
			{
				return 0;
			}

			*pbQuit = m_bQuit;
			return 0;

		};

		virtual	int32_t QueueWorkItem( boost::shared_ptr< TWorkItemBase > pItem, boost::shared_ptr< IThreadPoolCallback< TWorkItemBase > > pThreadPoolCallback )
		{
			boost::recursive_mutex::scoped_lock	lock( m_lockItemQueue );
			tagWorkItem< TWorkItemBase >	objWorkItem;

			if( m_deqItemQueue.size() >= m_uMaxQueueLen )
			{
				std::cout << "max_queue_len_reached" << std::endl;
				return -1;
			}

			objWorkItem.pItem = pItem;
			objWorkItem.pThreadPoolCallback = pThreadPoolCallback;
			m_deqItemQueue.push_back( objWorkItem );

			sem_post( &m_sem );

			return 0;
		};

		// 一定会从队列中弹出一项,否则不会返回
		virtual int32_t PopWorkItem( tagWorkItem< TWorkItemBase > *pobjWorkItem )
		{
			bool	bWorkItemEmpty = true;

			if( pobjWorkItem == NULL )
			{
				return -1;
			}

			while( true )
			{
				sem_wait( &m_sem );

				do
				{
					boost::recursive_mutex::scoped_lock	lock( m_lockItemQueue );

					if( m_deqItemQueue.empty() )
					{
						bWorkItemEmpty = true;
					}
					else
					{
						*pobjWorkItem = m_deqItemQueue.front();
						m_deqItemQueue.pop_front();
						bWorkItemEmpty = false;

						return 0;
					}

				}while( 0 ); // 用一个循环括住,这里是为了缩小锁的范围

				if( bWorkItemEmpty ) // 睡一段时间,让出 cpu, 同时让出队列的锁,好让别的地方可以操作队列
				{
					timespec  tv;

					tv.tv_sec = 0;
					tv.tv_nsec = ( long )( ( double )( 50.0 / 1000.0 ) * 1e+9 ); // nsec 单位是 1/1e-9 秒，sleep 50毫秒
					nanosleep( &tv, &tv );
				}
			}
		};

	protected:
		u_int32_t								m_uThreadCount;
		CWorkerThread< TWorkItemBase >			*m_pThread;

		u_int32_t								m_uMaxQueueLen; // 队列最大长度
		std::deque< tagWorkItem< TWorkItemBase > >	m_deqItemQueue;
		boost::recursive_mutex					m_lockItemQueue;
		sem_t									m_sem;

		bool									m_bQuit; // 是否需要退出
		boost::recursive_mutex					m_lockQuit;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IThreadPoolWorkItemBase
{
	public:
		IThreadPoolWorkItemBase(){};
		virtual	~IThreadPoolWorkItemBase(){};
};

#endif // _THREAD_POOL_INCLUDED_
