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

		virtual int32_t	Run( void *pArg ); // �����߳�
		virtual	int32_t	Stop(); // �����߳�
		virtual	int32_t	Join(); // �ȴ��߳̽���
		virtual	int32_t	IsRunning( bool *pbRunning ); // ����Ƿ�������

		virtual	void*	ThreadMain( void* pvArg ); // �����߳�ѭ������

	protected:
		virtual	int32_t	RunOnce(); // �����߳̿�ʼ���к�ִ��һ���ҽ�һ�Σ����ط� 0 ��ʱ���˳��߳�
		virtual	int32_t	RunTimes(); // �����߳̿�ʼ���к󷴸���ִ�У����ط� 0 ��ʱ���˳��߳�

	protected:
		bool				m_bRunning; // �Ƿ��������У��߳̿�ʼʱ����Ϊtrue���߳�ѭ���л������������false��ʱ������߳�ѭ��
		pthread_mutex_t		m_lockRunning; // ���� m_bRunning ����

		pthread_t			m_thread; // �����߳�
		tagThreadArg		m_objThreadArg;

		sem_t				m_sem; // ����û���κ�ʱ���ʱ�����������߳�
};

#endif
