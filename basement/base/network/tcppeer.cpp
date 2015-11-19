#include "tcppeer.h"


CPacketId::CPacketId()
{
	srand( time( NULL ) );
	m_uPacketId = 0;
	while( 0 == m_uPacketId ) 
	{
		m_uPacketId = rand();
	}
}

CPacketId::~CPacketId()
{
}

int32_t CPacketId::GeneratePacketId( u_int32_t *puPacketId )
{
	boost::mutex::scoped_lock lock( m_lockPacketId );

	if(  NULL == puPacketId )
	{
		return -1;
	}

	m_uPacketId ++;
	if( 0 == m_uPacketId  )
	{
		m_uPacketId++;
	}

	*puPacketId = m_uPacketId;

	return 0;
}


