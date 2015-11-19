#ifndef _TIMER_INCLUDED_
#define _TIMER_INCLUDED_

#include "thread_model.h"
#include "cppsink.h"
#include <vector>
#include <map>
#include <list>

#define	TIMER_MIN		(100.0) // 最小Interval，毫秒 

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

struct tagTimerItem // 每来一个AddTimer，就增加一项到list
{
	u_int32_t			uInterval; // 每隔多少毫秒触发一次
	bool				bOnce; // 是否只触发一次的
	u_int32_t			uTimerId;
	ITimerHandler		*pTimerHandler;
	void				*pvParam;
};
typedef std::vector< tagTimerItem >		CVecTimerItem;
typedef std::vector< CVecTimerItem >	CVecTimerChain;

struct tagTimerItemPos // 用于在 EraseTimer 的时候快速定位到要删除的东西
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

class CTimerDisp : public CThreadModel// 用于发出 OnTimer 调用
{
public:
	CTimerDisp();
	virtual ~CTimerDisp();

	virtual	int32_t	StartTimerDisp( CTimer *pTimer );
	virtual	int32_t	StopTimerDisp();

	virtual	int32_t	RunTimes();

protected:
	CTimer	*m_pTimer; // 绑定到的一个 Timer
};


class CTimer : public CThreadModel
{
public:
	friend class CTimerDisp;

public:
	CTimer();
	virtual ~CTimer();

	virtual	int32_t	StartTimer( u_int32_t uMaxInterval ); // 初始化的时候指定这个 Timer 所支持的最大时间间隔，单位毫秒
	virtual	int32_t	StopTimer();

	virtual	int32_t	SetTimer( u_int32_t uTimerId, u_int32_t uInterval, bool bOnce, ITimerHandler *pTimerHandler, void* pvParam );
	virtual int32_t	EraseTimer( u_int32_t uTimerId );

	virtual	int32_t	RunTimes();

protected:
	virtual	int32_t	GetTimerEvent( tagTimerEvent *pTimerEvent ); // 给 TimerDisp 调用的

protected:
	CVecTimerChain		m_vecTimerChain; // 十字链表,每隔 TIMER_MIN 毫秒后，将 m_lstTimerChain 的头部弹出一个
	CMapTimerItemPos	m_mapTimerItemPos; // 用于在 EraseTimer 的时候快速定位到要删除的东西
	u_int32_t			m_uTimerPos; // 当下一次 TIMER_MIN 间隔的 timer 触发后，需要对 m_vecTimerChain[ m_uTimerPos ] 发出 OnTimer 调用

	CLstTimerEvent		m_lstTimerEvent; // 当 timer 触发后，向这个队列增加内容；由 TimerDisp 线程负责取这个队列的项，发出OnTimer调用

	boost::recursive_mutex		m_mtxProtector;
	sem_t				m_semTimerEventCount;

	CTimerDisp			m_objTimerDisp;
};

#endif
