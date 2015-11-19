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

		// ����ʱ
		virtual	int32_t OnTimeout( boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ){ return -1; };
};

CPP_SINK_BEGIN( CTimeoutCallback ) 
	CPP_SINK_FUNC( int32_t, OnTimeout, ( boost::shared_array< u_int8_t > pchPacket, u_int32_t uPacketLen, u_int32_t uPacketId ), ( pchPacket, uPacketLen, uPacketId ) )
CPP_SINK_END()
DECLARE_CPP_SINK_OBJ( CTimeoutCallback, ITimeoutCallback )


// �������ʱ����
struct tagPacket
{
	boost::shared_array< u_int8_t >		pPacket; // ���ͳ�ȥ�İ�
	u_int32_t							uPacketLen; // ������
	u_int32_t							uPacketId;         // ������� packet_id
	timeval								tvSent; // �����������ʱ�䣬�� gettimeofday ���
	u_int32_t							uTimeout; // ���ٺ��볬ʱ
	timeval								tvTimeout; // ��ǰʱ����ڵ������ʱ�䣬��������ڶ��У�����Ϊ����ʱ
	boost::shared_ptr< ITimeoutCallback >	pTimeoutCallback; // ����������ʱ�����������ӿڵ� OnTimeout

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
typedef std::multimap< timeval, CLstPacket::iterator >	CMMapTimeout2Packet; // timeoutʱ��-->packet itr����ĳ��ʱ��Ҫ��ʱ�İ�

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

		// ��һ�� packet ���뵽��ʱ������,һ���� OnPacketSent ǰ������
		virtual int32_t AddPacket( boost::shared_array< u_int8_t > pPacket, u_int32_t uPacketLen,
				u_int32_t uPacketId, 
				u_int32_t uTimeout, boost::shared_ptr< ITimeoutCallback > pTimeoutCallback );

		// ��һ�� packet �ӳ�ʱ�������Ƴ�,һ���� OnPacketReceived �󱻵���
		virtual	int32_t	DelPacket( u_int32_t uPacketId ); 

		// �ӳ�һ�� packet �ĳ�ʱʱ��
		virtual	int32_t	ProlongPacket( u_int32_t uPacketId );

		// ��鳬ʱ�������ǲ�����ĳ����
		virtual int32_t IsPacketExist( u_int32_t uPacketId, bool *pbPacketExist );

		// ÿ�� timer ��ʱ����ã��� m_mmapTimeout ��ͷ��ʼ������ֱ��ĩβ�����ҵ� m_mmapTimeout.first ���� gettimeofday ��ʱ����ֹ
		// �Ա�����ÿ����߼��㷢�� OnTimeout �ص������Ž���Щ��� m_mmapTimeout ��ɾ��
		// ���timer�����ⲿά������ô�ⲿ��Ҫ���ڵĵ��� CheckTimeout
		virtual	int32_t	CheckTimeout(); 

		// timer �Ļص���ֻ��ʹ���ڲ� timer ʱ�����Ч
		// �п���ʹ���ⲿtimer��ԭ��apache��ʹ�ã������̶߳���apache��dso������������Ҫ��so��������Ϊso���̻߳ᱻ��ʱɱ��
		virtual	int32_t	OnTimer( u_int32_t uTimerId, void *pvParam );

	protected:

		// �� uPacketId �İ�������ʱ�ص������ҽ�������ӳ�ʱ������ɾ��
		virtual	int32_t	TimeoutCallback( u_int32_t uPacketId ); 

	protected:
		CLstPacket				m_lstPacket;
		CMapPacketId2Packet	m_mapPacketId;
		CMMapTimeout2Packet		m_mmapTimeout;


		boost::shared_ptr< CTimer >		m_pTimer;
		CTimerHandler    	   			m_objTimerHandler;
		timeval							m_tvTimerLast; // �ϴ� ontimer ��ʱ��
		u_int32_t						m_uTimerCount; // ������ٴ�timer��

	public:
		boost::recursive_mutex	m_lockTimeoutMgr;
};

#endif // _TIMEOUT_MGR_INCLUDED_
