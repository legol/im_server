#ifndef _NETWORK_UTIL_INCLUDED_
#define _NETWORK_UTIL_INCLUDED_

#include <string>
#include <sys/types.h>

#ifndef MAX_IP_LEN
	#define  MAX_IP_LEN     16
#endif
#ifndef MAX_HOST_NAME
	#define MAX_HOST_NAME  	256
#endif
#ifndef MAX_HOST_IP
	#define	MAX_HOST_IP    	8
#endif
#ifndef MD5_BUFLEN_CHAR
	#define MD5_BUFLEN_CHAR	16
#endif

class CLocalHost
{
	public:
		CLocalHost();
		~CLocalHost();

	public:
		int32_t     IsLocalIp(const char* pszIP);
		int32_t     IsLocalIp(u_int32_t nIP, bool bHost);
		int32_t     IsLocalName(const char* pszName);
		u_int32_t    GetLocalIp();	//网络字节序列

	public:
		char			m_szName[MAX_HOST_NAME];	//机器名称
		u_int32_t	m_nIP[MAX_HOST_IP];			//网络字节序的IP地址
		u_int32_t	m_nIPCount;
};

class CNetworkUtil
{
	public:
		static int      StrToIP  ( const std::string  str, u_int32_t& ip );
		static int      StrToIP  ( const char*        str, u_int32_t& ip );
		static int      IpToStr  ( const u_int32_t ip, std::string& str  );
		static char*    IpToStr  ( const u_int32_t ip, bool isNetOrder = true);
		static int      TimeToStr( const time_t time, std::string& str  );

		static void     SetNonBlockSocket( const int32_t fd );
		static void     SetBlockSocket   ( const int32_t fd );
		static int32_t  RegisterEvent    ( const int32_t epoll_fd, const int32_t fd, const u_int32_t event );
		static int32_t  UnregisterEvent  ( const int32_t epoll_fd, const int32_t fd );
		static int32_t  Bind( const int32_t fd, const char *ip, const u_int16_t port );
		static int32_t  GetPeerName( int fd, u_int32_t& ip, u_int16_t& port );
};

// uint64的主机-网络序转换，因为与hton*与ntoh*一族，保留该风格
u_int64_t htonll(u_int64_t hostValue);
u_int64_t ntohll(u_int64_t netValue);

#endif // _NETWORK_UTIL_INCLUDED_
