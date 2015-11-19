#include <sys/epoll.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <linux/unistd.h>
#include <time.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <string.h>

#include <string>
#include <vector>
#include <list>
#include <queue>

#include "networkutil.h"

CLocalHost::CLocalHost()
{
	m_szName[0] = 0;
	for( uint32_t i = 0; i < MAX_HOST_IP; i++ )
	{
		m_nIP[i] = 0;
	}

	gethostname( m_szName, MAX_HOST_NAME );
	m_szName[ MAX_HOST_NAME - 1 ] = 0;

	m_nIPCount = 0;
	struct hostent* pHost = gethostbyname( m_szName );
	if( NULL != pHost )
	{
		for( int32_t i = 0; pHost->h_addr_list[i] != NULL ; i++ )
		{
			if( pHost->h_length != sizeof( uint32_t ) )
			{
				continue;
			}

			memcpy( &m_nIP[m_nIPCount], pHost->h_addr_list[i], pHost->h_length );

			if( m_nIP[m_nIPCount] == 0x0100007f )
			{
				continue;
			}

			m_nIPCount ++;

			if( m_nIPCount == MAX_HOST_IP )
			{
				break;
			}
		}
	}
}

CLocalHost::~CLocalHost()
{
}

//判断是否本机器ip; 0-true; -1:fail
int32_t CLocalHost::IsLocalIp(const char* pszIP)
{
	return IsLocalIp(inet_addr(pszIP), false);
}

//bHost=true, 主机字节序
//判断是否本机器ip; 0-true; -1:fail
int32_t CLocalHost::IsLocalIp(uint32_t nIP, bool bHost)
{
	uint32_t	nIn;

	if(bHost)
	{
		nIn = htonl(nIP);
	}
	else
	{
		nIn = nIP;
	}

	for(uint32_t i = 0; i < m_nIPCount; i++)
	{
		if(m_nIP[i] == nIn)
		{
			return 0;
		}
	}

	return -1;
}

int32_t CLocalHost::IsLocalName(const char* pszName)
{
	if(strcasecmp(m_szName, pszName) == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

uint32_t CLocalHost::GetLocalIp()
{
	return 	m_nIP[0];
}


void  CNetworkUtil::SetNonBlockSocket( const int32_t fd )
{
	int32_t flag ;
	flag  = fcntl( fd, F_GETFL, 0 );
	flag |= O_NONBLOCK;
	flag |= O_NDELAY;
	fcntl( fd, F_SETFL, flag );
	return;
}

void  CNetworkUtil::SetBlockSocket(  const int32_t fd )
{
	int32_t flag;
	flag = fcntl( fd, F_GETFL, 0 );
	flag &= ~O_NONBLOCK;
	flag &= ~O_NDELAY;
	fcntl( fd, F_SETFL, flag );
	return ;
}

int32_t CNetworkUtil::RegisterEvent( const int32_t epoll_fd, const int32_t fd, const uint32_t event )
{
	epoll_event ev;
	bzero( &ev, sizeof(ev) );
	ev.data.fd = fd;
	ev.events  = event;
	return epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &ev );
}

int32_t CNetworkUtil::UnregisterEvent( const int32_t epoll_fd, const int32_t fd)
{
	return epoll_ctl( epoll_fd, EPOLL_CTL_DEL, fd, NULL );
}

int32_t CNetworkUtil::Bind( const int32_t fd, const char *ip, const uint16_t port )
{
	sockaddr_in server_addr;
	bzero( &server_addr, sizeof(server_addr) );
	server_addr.sin_family = AF_INET;
	inet_aton( ip, ( in_addr* )( &(server_addr.sin_addr.s_addr) ) );
	server_addr.sin_port   = htons( port ) ;

	int32_t result = bind( fd, ( sockaddr* ) &server_addr, sizeof(server_addr) );
	return  result;
}


int   CNetworkUtil::IpToStr( const uint32_t ip, std::string& str )
{
	char ipstr[16]     = {0};
	uint32_t p[4]      = {0};
	unsigned char *pip = (unsigned char *)&ip;

	for( int i = 0; i < 4; i++ )
	{        
		p[i] = pip[i];
	}

	int ret = snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
	str = ipstr;
	return (ret>0)? 0 : ret;
}

char* CNetworkUtil::IpToStr( const uint32_t ip, bool isNetOrder )
{
	// __thread supported at gcc 3.3 or above
	static /*__thread*/ char ipstr[16];
	uint32_t p[4]      = { 0 };
	unsigned char *pip = (unsigned char *)&ip;

	for( int i = 0; i < 4; i++ )
	{
		p[i] = pip[i];
	}

	if (isNetOrder)                                                                                                                
	{
		snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
	}   
	else
	{
		snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u", p[3], p[2], p[1], p[0]);
	}   

	return ipstr;
}


int CNetworkUtil::StrToIP( const char* str,uint32_t& ip)
{
	if(inet_pton(AF_INET,str,&ip) > 0)
	{
		return 0;
	}
	return -1;
}

int CNetworkUtil::StrToIP( const std::string str,uint32_t& ip)
{
	if(inet_pton(AF_INET,str.c_str(), &ip) > 0)
	{
		return 0;
	}
	return -1;
}

int CNetworkUtil::TimeToStr( const time_t time, std::string& str)
{
	char timestr[24];
	int ret;

	struct tm newTimeObj;
	struct tm *newTime = localtime_r(&time, &newTimeObj);

	// 2007-12-25 01:45:32
	ret = snprintf(timestr, sizeof(timestr), "%4d-%02d-%02d %02d:%02d:%02d"
			, newTime->tm_year + 1900, newTime->tm_mon + 1, newTime->tm_mday
			, newTime->tm_hour, newTime->tm_min, newTime->tm_sec);

	return ret;
}

int32_t  CNetworkUtil::GetPeerName( int fd, uint32_t& ip, uint16_t& port )
{
	struct sockaddr_in sa;
	int salen = sizeof(sa);   
	int ret = getpeername( fd,  (struct sockaddr*)&sa, (socklen_t*)&salen);
	if( ret != 0 )
	{
		return -1;
	}
	ip   = sa.sin_addr.s_addr;
	port = ntohs(sa.sin_port);
	return 0;
}

uint64_t htonll(uint64_t hostValue)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	return hostValue;
#else
	uint32_t lowValue  = (uint32_t)(hostValue & 0xFFFFFFFF) ;
	uint32_t highValue = (uint32_t)(hostValue >> 32);
	uint64_t netValue  = htonl(lowValue);                                                                                     
	netValue <<= 32; 
	netValue |= htonl(highValue);
	return netValue;
#endif
}

uint64_t ntohll(uint64_t netValue)                                                                                           
{
	return htonll( netValue );
}

