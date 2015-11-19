#ifndef _TIMEOUT_MGR_INCLUDED_
#define _TIMEOUT_MGR_INCLUDED_

#include <sys/types.h>
#include <list>
#include <map>
#include <boost/smart_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include "timer.h"
#include "cppsink.h"

bool operator < ( const timeval &_left, const timeval &_right );
bool operator > ( const timeval &_left, const timeval &_right );
const timeval operator-( const timeval &left, const timeval &right );

class ITimeoutCallback
{
	public:
		ITimeoutCallback(){};
		virtual ~ITimeoutCallback(){};

		// 包超时
		virtual	int32_t OnTimeout( boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ){ return -1; };
};

CPP_SINK_BEGIN( CTimeoutCallback ) 
	CPP_SINK_FUNC( int32_t, OnTimeout, ( boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ), ( pchPacket, uPacketLen, uPacketId ) )
CPP_SINK_END()
DECLARE_CPP_SINK_OBJ( CTimeoutCallback, ITimeoutCallback )


// 管理包超时的类
struct tagPacket
{
	boost::shared_array< u_int8_t >		pPacket; // 发送出去的包
	u_int32_t							uPacketLen; // 包长度
	u_int32_t							uPacketId;         // 这个包的 packet_id
	timeval								tvSent; // 发送这个包的时间，用 gettimeofday 获得
	u_int32_t							uTimeout; // 多少毫秒超时
	timeval								tvTimeout; // 当前时间大于等于这个时间，如果包还在队列，则认为包超时
	boost::shared_ptr< ITimeoutCallback >	pTimeoutCallback; // 如果这个包超时，则调用这个接口的 OnTimeout

	tagPacket()
	{
		uPacketLen = 0;
		memset( &tvSent, 0, sizeof( tvSent ) );
		uTimeout = 0;
		memset( &tvTimeout, 0, sizeof( tvTimeout ) );
	}
};



typedef std::list< tagPacket >							CLstPacket;
typedef std::map< u_int32_t, CLstPacket::iterator >		CMapPacketId2Packet; // packet id --> packet itr
typedef std::multimap< timeval, CLstPacket::iterator >	CMMapTimeout2Packet; // timeout时间-->packet itr，到某个时间要超时的包

class CTimeoutMgr;

class CTimerHandler : public ITimerHandler
{ 
	public: 
		CTimerHandler ( CTimeoutMgr *pHost ) 
		{ 
			m_pTimeoutMgr = pHost; 
			m_pfOnTimer = NULL;
		} 
		virtual ~CTimerHandler() {}

		typedef int32_t (CTimeoutMgr::*CallbackOnTimer)( u_int32_t uTimerId, void *pvParam ); 

		void Hook_OnTimer(CallbackOnTimer pfOnTimer) 
		{ 
			m_pfOnTimer= pfOnTimer; 
		} 

		virtual int32_t OnTimer( u_int32_t uTimerId, void *pvParam ) 
		{ 
			if (m_pTimeoutMgr && m_pfOnTimer) 
			{ 
				return ( (m_pTimeoutMgr->*m_pfOnTimer)(uTimerId, pvParam) ); 
			} 
			return -1; 
		} 

		CTimeoutMgr* m_pTimeoutMgr; 
	private: 
		CallbackOnTimer m_pfOnTimer; 
};


class CTimeoutMgr
{
	public:
		CTimeoutMgr();
		virtual ~CTimeoutMgr();

		virtual	int32_t	Init( boost::shared_ptr< CTimer > pTimer );

		// 将一个 packet 加入到超时队列中,一般在 OnPacketSent 前被调用
		virtual int32_t AddPacket( boost::shared_array< u_int8_t > pPacket, u_int32_t uPacketLen,
				u_int32_t uPacketId, 
				u_int32_t uTimeout, boost::shared_ptr< ITimeoutCallback > pTimeoutCallback );

		// 将一个 packet 从超时队列中移除,一般在 OnPacketReceived 后被调用
		virtual	int32_t	DelPacket( u_int32_t uPacketId ); 

		// 延长一个 packet 的超时时间
		virtual	int32_t	ProlongPacket( u_int32_t uPacketId );

		// 检查超时队列中是不是有某个包
		virtual int32_t IsPacketExist( u_int32_t uPacketId, bool *pbPacketExist );

		// 每次 timer 的时候调用，对 m_mmapTimeout 从头开始遍历，直到末尾或者找到 m_mmapTimeout.first 大于 gettimeofday 的时候终止
		// 对遍历的每项，向逻辑层发出 OnTimeout 回调，接着将这些项从 m_mmapTimeout 中删除
		// 如果timer是由外部维护，那么外部需要定期的调用 CheckTimeout
		virtual	int32_t	CheckTimeout(); 

		// timer 的回调，只有使用内部 timer 时候才有效
		// 有可能使用外部timer的原因：apache的使用，期望线程都在apache的dso里面启动，不要在so启动，因为so的线程会被随时杀死
		virtual	int32_t	OnTimer( u_int32_t uTimerId, void *pvParam );

	protected:

		// 对 uPacketId 的包发出超时回调，并且将这个包从超时队列中删除
		virtual	int32_t	TimeoutCallback( u_int32_t uPacketId ); 

	protected:
		CLstPacket				m_lstPacket;
		CMapPacketId2Packet	m_mapPacketId;
		CMMapTimeout2Packet		m_mmapTimeout;


		boost::shared_ptr< CTimer >		m_pTimer;
		CTimerHandler    	   			m_objTimerHandler;
		timeval							m_tvTimerLast; // 上次 ontimer 的时间
		u_int32_t						m_uTimerCount; // 至今多少次timer了

	public:
		boost::recursive_mutex	m_lockTimeoutMgr;
};

#endif // _TIMEOUT_MGR_INCLUDED_
