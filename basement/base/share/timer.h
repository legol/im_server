#ifndef _TIMER_INCLUDED_
#define _TIMER_INCLUDED_

#include "thread_model.h"
#include "cppsink.h"
#include <vector>
#include <map>
#include <list>

#define	TIMER_MIN		(100.0) // ��СInterval������ 

class ITimerHandler
{
public:
	ITimerHandler(){};
	virtual ~ITimerHandler(){};

	virtual	int32_t	OnTimer( u_int32_t uTimerId, void *pvParam ) = 0;
};

//CPP_SINK_BEGIN( CTimerHandler ) 
//  CPP_SINK_FUNC( int32_t, OnTimer, ( u_int32_t uTimerId, void *pvParam ), ( uTimerId, pvParam ) )
//CPP_SINK_END()
//DECLARE_CPP_SINK_OBJ( CTimerHandler, ITimerHandler )

struct tagTimerItem // ÿ��һ��AddTimer��������һ�list
{
	u_int32_t			uInterval; // ÿ�����ٺ��봥��һ��
	bool				bOnce; // �Ƿ�ֻ����һ�ε�
	u_int32_t			uTimerId;
	ITimerHandler		*pTimerHandler;
	void				*pvParam;
};
typedef std::vector< tagTimerItem >		CVecTimerItem;
typedef std::vector< CVecTimerItem >	CVecTimerChain;

struct tagTimerItemPos // ������ EraseTimer ��ʱ����ٶ�λ��Ҫɾ���Ķ���
{
	u_int32_t	uIdxX, uIdxY;
};
typedef	std::map< u_int32_t, tagTimerItemPos >	CMapTimerItemPos;

struct tagTimerEvent
{
	u_int32_t		uTimerId;
	void			*pvParam;
	ITimerHandler	*pTimerHandler;
};
typedef std::list< tagTimerEvent >	CLstTimerEvent;


class CTimer;
class CTimerDisp;

class CTimerDisp : public CThreadModel// ���ڷ��� OnTimer ����
{
public:
	CTimerDisp();
	virtual ~CTimerDisp();

	virtual	int32_t	StartTimerDisp( CTimer *pTimer );
	virtual	int32_t	StopTimerDisp();

	virtual	int32_t	RunTimes();

protected:
	CTimer	*m_pTimer; // �󶨵���һ�� Timer
};


class CTimer : public CThreadModel
{
public:
	friend class CTimerDisp;

public:
	CTimer();
	virtual ~CTimer();

	virtual	int32_t	StartTimer( u_int32_t uMaxInterval ); // ��ʼ����ʱ��ָ����� Timer ��֧�ֵ����ʱ��������λ����
	virtual	int32_t	StopTimer();

	virtual	int32_t	SetTimer( u_int32_t uTimerId, u_int32_t uInterval, bool bOnce, ITimerHandler *pTimerHandler, void* pvParam );
	virtual int32_t	EraseTimer( u_int32_t uTimerId );

	virtual	int32_t	RunTimes();

protected:
	virtual	int32_t	GetTimerEvent( tagTimerEvent *pTimerEvent ); // �� TimerDisp ���õ�

protected:
	CVecTimerChain		m_vecTimerChain; // ʮ������,ÿ�� TIMER_MIN ����󣬽� m_lstTimerChain ��ͷ������һ��
	CMapTimerItemPos	m_mapTimerItemPos; // ������ EraseTimer ��ʱ����ٶ�λ��Ҫɾ���Ķ���
	u_int32_t			m_uTimerPos; // ����һ�� TIMER_MIN ����� timer ��������Ҫ�� m_vecTimerChain[ m_uTimerPos ] ���� OnTimer ����

	CLstTimerEvent		m_lstTimerEvent; // �� timer ����������������������ݣ��� TimerDisp �̸߳���ȡ������е������OnTimer����

	boost::recursive_mutex		m_mtxProtector;
	sem_t				m_semTimerEventCount;

	CTimerDisp			m_objTimerDisp;
};

#endif
