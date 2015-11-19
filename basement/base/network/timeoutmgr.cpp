#include "timeoutmgr.h"
#include "networkutil.h"
#include <sys/time.h>
#include <set>

bool operator < ( const timeval &_left, const timeval &_right )
{
	if( _left.tv_sec < _right.tv_sec )
	{
		return true;
	}
	else if ( _left.tv_sec > _right.tv_sec )
	{
		return false;
	}
	else
	{
		if( _left.tv_usec < _right.tv_usec )
		{
			return true;
		}
		else if( _left.tv_usec > _right.tv_usec )
		{
			return false;
		}
		else
		{
			return false;
		}
	}
}

bool operator > ( const timeval &_left, const timeval &_right )
{ 
	if( _left.tv_sec == _right.tv_sec && _left.tv_usec == _right.tv_usec )
	{
		return false;
	}
	else
	{
		if( _right < _left )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}


const timeval operator-( const timeval &left, const timeval &right )
{
	timeval tvResult = { 0 };
	int64_t n64USeconds = 0, n64Seconds = 0;

	n64Seconds = ( int64_t )left.tv_sec - ( int64_t )right.tv_sec;
	n64USeconds = ( int64_t )left.tv_usec - ( int64_t )right.tv_usec;

	while( n64USeconds < 0 )
	{
		n64Seconds--;
		n64USeconds += ( int64_t )1e6;
	}

	if( n64Seconds < 0 || ( n64Seconds == 0 && n64USeconds < 0 ) )
	{
		return tvResult;
	}

	tvResult.tv_sec = n64Seconds;
	tvResult.tv_usec = n64USeconds;

	return tvResult;  
};





const u_int32_t TIMEOUT_INTERVAL = 100;
const u_int32_t TIMER_ID_TIMEOUT = 19800202;

CTimeoutMgr::CTimeoutMgr() : m_objTimerHandler( this )
{
	m_uTimerCount = 0;
	memset( &m_tvTimerLast, 0, sizeof( m_tvTimerLast ) );
}

CTimeoutMgr::~CTimeoutMgr()
{
	m_pTimer->StopTimer();
}

int32_t	CTimeoutMgr::OnTimer( u_int32_t uTimerId, void *pvParam )
{
	boost::recursive_mutex::scoped_lock lock( m_lockTimeoutMgr );

	if( TIMER_ID_TIMEOUT == uTimerId )
	{
		CheckTimeout();
	}
	return 0;
}

int32_t	CTimeoutMgr::TimeoutCallback( u_int32_t uPacketId )
{
	boost::recursive_mutex::scoped_lock lock( m_lockTimeoutMgr );

	CMapPacketId2Packet::iterator   itrPacketId;
	CLstPacket::iterator            itrPacket;

	tagPacket                       objPacket;

	itrPacketId = m_mapPacketId.find( uPacketId );
	if( itrPacketId == m_mapPacketId.end() )
	{
		return -1;
	}

	itrPacket = itrPacketId->second;
	objPacket = *itrPacket;

	if ( objPacket.pTimeoutCallback.get() != NULL )
	{
		objPacket.pTimeoutCallback->OnTimeout( objPacket.pPacket, objPacket.uPacketLen, objPacket.uPacketId );
	}

	return 0;
}

int32_t	CTimeoutMgr::AddPacket( boost::shared_array< u_int8_t > pPacket, u_int32_t uPacketLen,
		u_int32_t uPacketId,
		u_int32_t uTimeout, boost::shared_ptr< ITimeoutCallback > pTimeoutCallback )
{
	boost::recursive_mutex::scoped_lock lock( m_lockTimeoutMgr );

	tagPacket		objPacket;
	//	timeval			tvInterval;

	// 如果这个包已经存在了，则发出回调先，然后删除之
	TimeoutCallback( uPacketId );
	DelPacket( uPacketId );

	objPacket.pPacket    = pPacket;
	objPacket.uPacketLen = uPacketLen;
	objPacket.uPacketId = uPacketId;
	gettimeofday( &( objPacket.tvSent ), NULL );
	objPacket.uTimeout = uTimeout;
	objPacket.tvTimeout.tv_sec = objPacket.tvSent.tv_sec + ( uTimeout / 1000 );
	objPacket.tvTimeout.tv_usec = objPacket.tvSent.tv_usec + ( ( uTimeout % 1000 ) * 1000 );

	objPacket.tvTimeout.tv_sec = objPacket.tvTimeout.tv_sec + objPacket.tvTimeout.tv_usec / 1000000;
	objPacket.tvTimeout.tv_usec = objPacket.tvTimeout.tv_usec % 1000000;

	objPacket.pTimeoutCallback = pTimeoutCallback;

	m_lstPacket.push_front( objPacket );
	m_mapPacketId[ uPacketId ] = m_lstPacket.begin();
	m_mmapTimeout.insert( std::make_pair( objPacket.tvTimeout, m_lstPacket.begin() ) );

	return 0;
}

int32_t	CTimeoutMgr::DelPacket( u_int32_t uPacketId )
{
	boost::recursive_mutex::scoped_lock lock( m_lockTimeoutMgr );

	CMapPacketId2Packet::iterator				itrPacketId;
	CMMapTimeout2Packet::iterator				itrTimeout;
	CLstPacket::iterator						itrPacket;
	std::pair< CMMapTimeout2Packet::iterator, CMMapTimeout2Packet::iterator >	prTimeout;

	tagPacket                                   objPacket;

	itrPacketId = m_mapPacketId.find( uPacketId );
	if( itrPacketId != m_mapPacketId.end() )
	{
		itrPacket = itrPacketId->second;
		objPacket = *itrPacket;

		// 从 m_mmapTimeout 中删除
		prTimeout = m_mmapTimeout.equal_range( objPacket.tvTimeout );
		for( itrTimeout = prTimeout.first; itrTimeout != prTimeout.second; itrTimeout++ )
		{
			if( itrTimeout->second == itrPacketId->second )
			{
				m_mmapTimeout.erase( itrTimeout );
				break;
			}
		}

		// 从 m_mapPacketId 中删除
		m_mapPacketId.erase( itrPacketId );

		// 从 m_lstPacket 中删除
		m_lstPacket.erase( itrPacket );
	}
	else
	{
		return -1;
	}

	return 0;
}

int32_t	CTimeoutMgr::CheckTimeout()
{
	boost::recursive_mutex::scoped_lock lock( m_lockTimeoutMgr );

	CMMapTimeout2Packet::iterator	itrTimeout, itrTimeoutFootprint;
	CLstPacket::iterator			itrPacket;
	tagPacket						objPacket;

	std::set< u_int32_t	>    		setTimeoutPacketId;

	timeval							tvCurrent, tvInterval;

	gettimeofday( &tvCurrent, NULL );

	itrTimeout = m_mmapTimeout.begin();
	while( itrTimeout != m_mmapTimeout.end() )
	{
		if( itrTimeout->first < tvCurrent || ( itrTimeout->first.tv_sec == tvCurrent.tv_sec && itrTimeout->first.tv_usec == tvCurrent.tv_usec ) )
		{
			tvInterval = tvCurrent - itrTimeout->first;
			itrPacket  = itrTimeout->second;
			objPacket  = *itrPacket;

			setTimeoutPacketId.insert( objPacket.uPacketId );

			if( objPacket.pTimeoutCallback.get() != NULL )
			{
				//				printf( "timeout: packet_id=%u, tv_timeout=%lu[%lu], tv_current=%lu[%lu], interval=%lu[%lu]\n",
				//								objPacket.uPacketId,
				//								itrTimeout->first.tv_sec, itrTimeout->first.tv_usec,
				//								tvCurrent.tv_sec, tvCurrent.tv_usec,
				//								tvInterval.tv_sec, tvInterval.tv_usec
				//								);
				objPacket.pTimeoutCallback->OnTimeout( objPacket.pPacket, objPacket.uPacketLen, objPacket.uPacketId );
			}

			itrTimeoutFootprint = itrTimeout;
			itrTimeout++;
			m_mmapTimeout.erase( itrTimeoutFootprint );
		}
		else
		{
			break;
		}
	}

	for( std::set< u_int32_t >::iterator itr = setTimeoutPacketId.begin(); itr != setTimeoutPacketId.end(); itr++ )
	{
		DelPacket( *itr );
	}

	return 0;
}

int32_t	CTimeoutMgr::Init( boost::shared_ptr< CTimer > pTimer )
{
	m_objTimerHandler.Hook_OnTimer( &CTimeoutMgr::OnTimer );

	if( NULL == pTimer.get() )
	{
		return -1;
	}

	m_pTimer = pTimer;
	m_pTimer->SetTimer( TIMER_ID_TIMEOUT, TIMEOUT_INTERVAL, false, &m_objTimerHandler, NULL );

	return 0;
}

int32_t	CTimeoutMgr::IsPacketExist( u_int32_t uPacketId, bool *pbPacketExist )
{
	boost::recursive_mutex::scoped_lock lock( m_lockTimeoutMgr );

	CMapPacketId2Packet::iterator 	itr;

	if( NULL == pbPacketExist )
	{
		return -1;
	}

	itr = m_mapPacketId.find( uPacketId );
	if( itr == m_mapPacketId.end() )
	{
		*pbPacketExist = false;
	}
	else
	{
		*pbPacketExist = true;
	}

	return 0;
}
		
int32_t	CTimeoutMgr::ProlongPacket( u_int32_t uPacketId )
{
	boost::recursive_mutex::scoped_lock lock( m_lockTimeoutMgr );

	CMapPacketId2Packet::iterator 	itrPacketId;
	CLstPacket::iterator			itrPacket;
	CMMapTimeout2Packet::iterator	itrTimeout;

	std::pair< CMMapTimeout2Packet::iterator, CMMapTimeout2Packet::iterator >	prTimeout;

	itrPacketId = m_mapPacketId.find( uPacketId );
	if( itrPacketId == m_mapPacketId.end() )
	{
		return -1;
	}

	itrPacket = itrPacketId->second;

	// 从 m_mmapTimeout 中删除
	prTimeout = m_mmapTimeout.equal_range( ( *itrPacket ).tvTimeout );
	for( itrTimeout = prTimeout.first; itrTimeout != prTimeout.second; itrTimeout++ )
	{
		if( itrTimeout->second == itrPacketId->second )
		{
			m_mmapTimeout.erase( itrTimeout );
			break;
		}
	}

	// 重新到 m_mmapTimeout
	gettimeofday( &( ( *itrPacket ).tvSent ), NULL );
	
	( *itrPacket ).tvTimeout.tv_sec = ( *itrPacket ).tvSent.tv_sec + ( ( *itrPacket ).uTimeout / 1000 );
	( *itrPacket ).tvTimeout.tv_usec = ( *itrPacket ).tvSent.tv_usec + ( ( ( *itrPacket ).uTimeout % 1000 ) * 1000 );

	( *itrPacket ).tvTimeout.tv_sec = ( *itrPacket ).tvTimeout.tv_sec + ( *itrPacket ).tvTimeout.tv_usec / 1000000;
	( *itrPacket ).tvTimeout.tv_usec = ( *itrPacket ).tvTimeout.tv_usec % 1000000;

	m_mmapTimeout.insert( std::make_pair( ( *itrPacket ).tvTimeout, itrPacket ) );

	return 0;
}

