#include "timer.h" 
#include <math.h>
#include <stdio.h>

CTimer::CTimer() 
{
	m_uTimerPos = 0;
}

CTimer::~CTimer()
{
}

int32_t CTimer::StartTimer( u_int32_t uMaxInterval )
{
	boost::recursive_mutex::scoped_lock	lock( m_mtxProtector );	

	u_int32_t	uTimerChainSize = 0;

	uTimerChainSize = ( u_int32_t )( ( ( double )uMaxInterval / ( double )TIMER_MIN ) + 1.0 );
	m_vecTimerChain.resize( uTimerChainSize );

	sem_init( &m_semTimerEventCount, 0, 0 );

	m_objTimerDisp.StartTimerDisp( this );
	m_objTimerDisp.Run( NULL );

	return Run( NULL );
}

int32_t CTimer::StopTimer()
{
	boost::recursive_mutex::scoped_lock	lock( m_mtxProtector );

	sem_destroy( &m_semTimerEventCount );
	m_objTimerDisp.StopTimerDisp();

	return Stop();
}

int32_t CTimer::RunTimes()
{
	timespec				tv;
	tagTimerEvent		objTimerEvent;

	tv.tv_sec = 0;
	tv.tv_nsec = ( long )( ( double )( TIMER_MIN / 1000.0 ) * 1e+9 ); // nsec ��λ�� 1/1e-9 �룬sleep 50����
	nanosleep( &tv, &tv );

	{
		boost::recursive_mutex::scoped_lock	lock( m_mtxProtector );	
		u_int32_t							uInsertionPos; // ��once�ģ��ٴβ��뵽 TimerChain ��ʱ���λ��
		tagTimerItemPos						objTimerItemPos;

		if( m_uTimerPos < m_vecTimerChain.size() )
		{
			CVecTimerItem		vecTimerItem, vecNonOnceTimerItem;

			// ���� OnTimer������� once �� TimerItem��ɾ�� vecTimerItem
			vecTimerItem = m_vecTimerChain[ m_uTimerPos ];
			for( CVecTimerItem::iterator itr = vecTimerItem.begin(); itr != vecTimerItem.end(); itr++ )
			{
				if ( (*itr).bOnce == false )
				{
					vecNonOnceTimerItem.push_back( *itr );
				}

				objTimerEvent.uTimerId = (*itr).uTimerId;
				objTimerEvent.pvParam = (*itr).pvParam;
				objTimerEvent.pTimerHandler = (*itr).pTimerHandler;
				m_lstTimerEvent.push_back( objTimerEvent ); // OnTimer �ĵ�������һ���߳̽���
				sem_post( &m_semTimerEventCount );

				m_mapTimerItemPos.erase( (*itr).uTimerId ); // timerid ��Ӧ�� timeritem��λ����Ҫ����
			}
			m_vecTimerChain[ m_uTimerPos ].clear();

			// ���� once ���ٴμ��뵽 m_vecTimerChain
			for( CVecTimerItem::iterator itr = vecNonOnceTimerItem.begin(); itr != vecNonOnceTimerItem.end(); itr++ )
			{
				uInsertionPos = ( u_int32_t )( m_uTimerPos + 1 + ( (*itr).uInterval / TIMER_MIN ) ) % ( ( u_int32_t )( m_vecTimerChain.size() ) );
				m_vecTimerChain[ uInsertionPos ].push_back( *itr );

				objTimerItemPos.uIdxX = uInsertionPos;
				objTimerItemPos.uIdxY = m_vecTimerChain[ uInsertionPos ].size() - 1;
				m_mapTimerItemPos[ (*itr).uTimerId ] = objTimerItemPos;
			}

			m_uTimerPos = ( m_uTimerPos + 1 ) % m_vecTimerChain.size();
		}
	}

	return 0;
}

int32_t	CTimer::SetTimer( u_int32_t uTimerId, u_int32_t uInterval, bool bOnce, ITimerHandler *pTimerHandler, void* pvParam )
{
	boost::recursive_mutex::scoped_lock	lock( m_mtxProtector );

	u_int32_t										uInsertionPos;
	tagTimerItem								objTimerItem;

	CMapTimerItemPos::iterator	itrPos;
	tagTimerItemPos							objTimerItemPos;

	bool												bRunning = false;	

	IsRunning( &bRunning );
	if( bRunning == false )
	{
		return -1;
	}

	itrPos = m_mapTimerItemPos.find( uTimerId );
	if( itrPos != m_mapTimerItemPos.end() )
	{
		// ��������֮ǰ�Ѿ��е� timer
		return -1;
	}

	objTimerItem.uTimerId = uTimerId;
	objTimerItem.uInterval = uInterval;
	objTimerItem.bOnce = bOnce;
	objTimerItem.pTimerHandler = pTimerHandler;
	objTimerItem.pvParam = pvParam;

	uInsertionPos = ( u_int32_t )( m_uTimerPos + round( uInterval / TIMER_MIN ) ) % ( ( u_int32_t )( m_vecTimerChain.size() ) );
	m_vecTimerChain[ uInsertionPos ].push_back( objTimerItem );

	objTimerItemPos.uIdxX = uInsertionPos;
	objTimerItemPos.uIdxY = m_vecTimerChain[ uInsertionPos ].size() - 1;
	m_mapTimerItemPos[ uTimerId ] = objTimerItemPos;

	return 0;
}

int32_t	CTimer::EraseTimer( u_int32_t uTimerId )
{
	boost::recursive_mutex::scoped_lock	lock( m_mtxProtector );

	CMapTimerItemPos::iterator	itrPos;
	CLstTimerEvent::iterator		itr, itrNext;

	itrPos = m_mapTimerItemPos.find( uTimerId );
	if( itrPos == m_mapTimerItemPos.end() )
	{
		return -1;
	}

	m_vecTimerChain[ itrPos->second.uIdxX ].erase( m_vecTimerChain[ itrPos->second.uIdxX ].begin() + itrPos->second.uIdxY );

	// �� timerevent �����еĶ�Ӧ timerid ��ɾ��
	itr = m_lstTimerEvent.begin();
	while ( itr != m_lstTimerEvent.end() )
	{
		itrNext = itr;
		itrNext++;
		if( ( *itr ).uTimerId != uTimerId )
		{
			m_lstTimerEvent.erase( itr );
		}

		itr = itrNext;
	}

	m_mapTimerItemPos.erase( itrPos );

	return 0;
}

int32_t	CTimer::GetTimerEvent( tagTimerEvent *pTimerEvent )
{
	sem_wait( &m_semTimerEventCount );

	do
	{
		{
			boost::recursive_mutex::scoped_lock	lock( m_mtxProtector );

			if( m_lstTimerEvent.empty() == false )
			{
				*pTimerEvent = *( m_lstTimerEvent.begin() );
				m_lstTimerEvent.pop_front();
				break;
			}
		}

		{
			printf( "ûȡ��timerevent��˯100ms\n" );
			timespec  tv;

			tv.tv_sec = 0;
			tv.tv_nsec = ( long )( ( double )( 50.0 / 1000.0 ) * 1e+9 ); // nsec ��λ�� 1/1e-9 �룬sleep 50����
			nanosleep( &tv, &tv );

			continue;
		}

	}while( true );

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTimerDisp::CTimerDisp()
{
}

CTimerDisp::~CTimerDisp()
{
}

int32_t	CTimerDisp::RunTimes()
{
	tagTimerEvent	objTimerEvent;

	if( m_pTimer == NULL )
	{
		return -1;
	}

	m_pTimer->GetTimerEvent( &objTimerEvent );
	objTimerEvent.pTimerHandler->OnTimer( objTimerEvent.uTimerId, objTimerEvent.pvParam );

	return 0;
}

int32_t	CTimerDisp::StartTimerDisp( CTimer *pTimer )
{
	m_pTimer = pTimer;
	return 0;
}

int32_t	CTimerDisp::StopTimerDisp()
{
	return Stop();
}
