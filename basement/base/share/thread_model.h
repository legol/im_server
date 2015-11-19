#ifndef _THREAD_MODEL_INCLUDED_
#define _THREAD_MODEL_INCLUDED_

#include <pthread.h>
#include <semaphore.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/mutex.hpp>

class CThreadModel;

struct tagThreadArg
{
	CThreadModel	*pThis;
	void				*pArg;

	tagThreadArg()
	{
		pThis = NULL;
		pArg = NULL;
	}

	tagThreadArg( CThreadModel *_pThis, void *_pArg )
	{
		pThis = _pThis;
		pArg = _pArg;
	}
};


class CThreadModel
{
	public:
		CThreadModel();
		virtual ~CThreadModel();

		virtual	int32_t	Create(); 
		virtual	int32_t	FreeAll();

		virtual int32_t	Run( void *pArg ); // 启动线程
		virtual	int32_t	Stop(); // 结束线程
		virtual	int32_t	Join(); // 等待线程结束
		virtual	int32_t	IsRunning( bool *pbRunning ); // 检测是否在运行

		virtual	void*	ThreadMain( void* pvArg ); // 工作线程循环函数

	protected:
		virtual	int32_t	RunOnce(); // 会在线程开始运行后执行一次且仅一次，返回非 0 的时候退出线程
		virtual	int32_t	RunTimes(); // 会在线程开始运行后反复的执行，返回非 0 的时候退出线程

	protected:
		bool				m_bRunning; // 是否正在运行，线程开始时候被置为true，线程循环中会检测这个变量，false的时候结束线程循环
		pthread_mutex_t		m_lockRunning; // 操作 m_bRunning 的锁

		pthread_t			m_thread; // 工作线程
		tagThreadArg		m_objThreadArg;

		sem_t				m_sem; // 用在没有任何时间的时候，阻塞工作线程
};

#endif
