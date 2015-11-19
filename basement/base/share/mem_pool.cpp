#include "mem_pool.h"
#include <stdio.h>
#include <boost/regex.hpp>

CMemPool::CMemPool()
{
}

CMemPool::~CMemPool()
{
	Destroy();
}

///**
//* @param [in] strMemPoolCfg	  the config string in format as (size1,count1),(size2,count2)...
//*  for example : (1024,10000),(2048,10000),(4096,1000),(8192,1000),(16384,1000),(32768,1000),(65536,100),(131072,100),(262144,100)
//*/
int32_t CMemPool::Init( const std::string &strMemPoolCfg )
{
	// there is no boost lib under "linux kernel 2.4 and gcc 2.96 "...
	boost::regex  regex( "\\s*\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\)\\s*", boost::regbase::icase );
	boost::smatch matches;
	std::string::const_iterator itrBegin = strMemPoolCfg.begin(),itrEnd = strMemPoolCfg.end();

	CMapMemPoolConfig	mapMemPoolCfg;
	u_int32_t			uSize, uCount;

	std::string str1, str2;
	while ( boost::regex_search( itrBegin, itrEnd, matches, regex, boost::regex_constants::match_single_line ) )
	{
		if( matches[ 1 ].matched && matches[ 2 ].matched )
		{
			str1 = matches[ 1 ];
			str2 = matches[ 2 ];

			if( str1.empty() == false && str2.empty() == false )
			{
				uSize = atoi( str1.c_str() );
				uCount = atoi( str2.c_str() );
				mapMemPoolCfg[ uSize ] = uCount;
			}
		}

		itrBegin = matches[ 0 ].second;
	}

	return Init( mapMemPoolCfg );

	// pcre 必需要自己编译，系统带的无法连接
	//	pcre 			*re;
	//	const char 		*error;
	//	int				erroffset;
	//	const u_int32_t	c_uMaxMatchCount = 1000;
	//	int 			ovector[ 3 * c_uMaxMatchCount ];
	//	int 			rc;
	//
	//	std::string		strRegex = "\\s*\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\)\\s*";
	//
	//	/* Compile the regular expression in the first argument */
	//	re = pcre_compile(
	//			strRegex.c_str(),     /* the pattern */
	//			PCRE_CASELESS,           /* options */
	//			&error,      /* for error message */
	//			&erroffset,  /* for error offset */
	//			NULL);       /* use default character tables */
	//
	//	/* Compilation failed: print the error message and exit */
	//	if (re == NULL)
	//	{
	//		std::cout << "regex format wrong:" << error << std::endl;
	//		return -1;
	//	}
	//
	//	CMapMemPoolConfig	mapMemPoolCfg;
	//	u_int32_t			uStart;
	//	u_int32_t			uSize, uCount;
	//	std::string 		str1, str2;
	//
	//	uStart = 0;
	//	while( uStart < strMemPoolCfg.length() &&
	//			( rc = pcre_exec( re,          /* the compiled pattern */
	//							NULL,        /* we didn't study the pattern */
	//							strMemPoolCfg.c_str(),
	//							strMemPoolCfg.length(),
	//							uStart,           /* start at offset 0 in the subject */
	//							0,           /* default options */
	//							ovector,     /* vector for substring information */
	//							3 * c_uMaxMatchCount ) ) > 0 )  /* number of elements in the vector */
	//	{
	//		str1.assign( strMemPoolCfg.c_str() + ovector[2*1], ovector[2*1+1] - ovector[2*1] ); 
	//		str2.assign( strMemPoolCfg.c_str() + ovector[2*2], ovector[2*2+1] - ovector[2*2] ); 
	//
	//		if( str1.empty() == false && str2.empty() == false )
	//		{
	//		   	uSize = atoi( str1.c_str() );
	//			uCount = atoi( str2.c_str() );
	//			mapMemPoolCfg[ uSize ] = uCount;
	//		}
	//
	//		uStart = ovector[ 2 * 0 + 1 ]; // ovector[ 0 ] 和 ovector[ 1 ] 是整个匹配串的开始和结束
	//	}
	//
	//	return Init( mapMemPoolCfg );
}

int32_t CMemPool::Init( const CMapMemPoolConfig &mapConfig )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );

	tagMemPool	objMemPool;

	m_mapConfig = mapConfig;
	for( CMapMemPoolConfig::const_iterator itr = m_mapConfig.begin(); itr != m_mapConfig.end(); itr++ )
	{
		objMemPool.uCount = 0;
		objMemPool.pMemPoolBase = boost::shared_ptr< CMemPoolBase >( new ( std::nothrow ) CMemPoolBase( sizeof( tagMemItem ) + itr->first ) );
		m_mapMemPool[ itr->first ] = objMemPool;
	}


	return 0;
}

int32_t CMemPool::Destroy()
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );

	m_mapMemPool.clear();
	m_mapConfig.clear();
	return 0;
}

u_int8_t* CMemPool::Malloc_Internal( size_t szSize,size_t* allocated )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );


	CMapMemPool::iterator itr = m_mapMemPool.end();

	for( itr = m_mapMemPool.begin(); itr != m_mapMemPool.end(); itr++  )
	{
		if( itr->first >= szSize )
		{
			break;
		}
	}

	if( itr != m_mapMemPool.end() )
	{
		CMapMemPoolConfig::iterator			itrConfig;
		boost::shared_ptr< CMemPoolBase >	pMemPoolBase;

		itrConfig = m_mapConfig.find( itr->first );
		if( itrConfig == m_mapConfig.end() )
		{
			// 不应该进入这里
			return NULL;
		}

		if( itr->second.pMemPoolBase.get() == NULL )
		{
			// 不应该进入这里
			return NULL;
		}

		if( itr->second.uCount == itrConfig->second ) // 达到上限
		{
			return NULL;
		}

		itr->second.uCount++;
		pMemPoolBase = itr->second.pMemPoolBase;

		tagMemItem	*pMemItem = ( tagMemItem* )( pMemPoolBase->malloc() );
		if( pMemItem == NULL )
		{
			// 内存不足，一般不会进入这里，因为在前面已经有内存块分配个数的判断
			return NULL;
		}

		if ( allocated )
			*allocated = itr->first;

		pMemItem->pMemPool = this;
		pMemItem->szSize = szSize;
		return pMemItem->pPointer;
	}

	return NULL;
}

void CMemPool::Free_Internal( u_int8_t* pPointer )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );

	tagMemItem	*pMemItem = NULL;
	CMapMemPool::iterator				itrMemPool;
	size_t								uBlockSize;
	bool								bFound = false;

	if( pPointer == NULL )
	{
		return;
	}

	pMemItem = ( tagMemItem* )( pPointer - sizeof( tagMemItem ) );
	if( pMemItem->pMemPool != this )
	{
		// 不应该进入这里
		return;
	}

	for( CMapMemPoolConfig::iterator itr = m_mapConfig.begin(); itr!= m_mapConfig.end(); itr++ )
	{
		uBlockSize = itr->first;
		if( uBlockSize >= pMemItem->szSize )
		{
			bFound = true;
			break;
		}
	}

	if( bFound == false )
	{
		// 不应该进入这里
		return;
	}

	itrMemPool = m_mapMemPool.find( uBlockSize );
	if( itrMemPool == m_mapMemPool.end() )	
	{
		// 不应该进入这里
		return;
	}

	if( itrMemPool->second.pMemPoolBase.get() == NULL )
	{
		// 不应该进入这里
		return;
	}

	if( itrMemPool->second.uCount == 0 )
	{
		// 不应该进入这里
		return;
	}

	itrMemPool->second.uCount--;
	itrMemPool->second.pMemPoolBase->free( pMemItem );
}

boost::shared_array< u_int8_t > CMemPool::Malloc( size_t szSize,size_t* allocated )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );

	u_int8_t	*pPointerRaw = Malloc_Internal( szSize,allocated );	

	if( pPointerRaw == NULL )
	{
		return boost::shared_array< u_int8_t >();
	}
	else
	{
		return boost::shared_array< u_int8_t >( pPointerRaw, CMemPool::MemItem_Deletor );
	}
}

void CMemPool::MemItem_Deletor( u_int8_t* pPointer )
{
	tagMemItem  *pMemItem = ( tagMemItem* )( pPointer - sizeof( tagMemItem ) );
	pMemItem->pMemPool->Free_Internal( pPointer );
}


