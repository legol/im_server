#ifndef _MEM_POOL_INCLUDED_
#define _MEM_POOL_INCLUDED_

#ifndef __cplusplus
#define __cplusplus
#endif

#include <map>
#include <sys/types.h>
#include <string>
#include <boost/pool/pool.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>

typedef boost::pool<> CMemPoolBase;

struct tagMemPool
{
	u_int32_t uCount;
	boost::shared_ptr< CMemPoolBase > pMemPoolBase;

	tagMemPool()
	{
		uCount = 0;
	}

	tagMemPool( const tagMemPool &right )
	{
		uCount = right.uCount;
		pMemPoolBase = right.pMemPoolBase;
	}

	~tagMemPool()
	{
		uCount = 0;
		pMemPoolBase.reset();
	}

	const tagMemPool& operator=( const tagMemPool &right )
	{
		uCount = right.uCount;
		pMemPoolBase = right.pMemPoolBase;
		return *this;
	}
};

/**
 *  @CMapMemPoolConfig		内存块大小－个数
 */
typedef std::map< size_t, u_int32_t >		CMapMemPoolConfig;

/**
 *  @CMapMemPool		内存块大小－（当前个数，对应的boost内存池）
 */
typedef std::map< size_t, tagMemPool >	CMapMemPool; 

class CMemPool;

struct tagMemItem
{
	CMemPool	*pMemPool; // 分配这块内存的内存池指针
	size_t		szSize; // 这块内存的大小,即 pPointer指向的内存块大小
	u_int8_t	pPointer[]; // 内存块
};

class CMemPool
{
	public:
		CMemPool();
		virtual ~CMemPool();

		virtual	int32_t		Init( const CMapMemPoolConfig &mapConfig ); // 该函数只允许被调用一次
		virtual	int32_t		Init( const std::string &strMemPoolCfg ); // 接受这样格式的字符串( size, count ), ( size, count )...
		virtual	int32_t 	Destroy();
		void							Dump();

		virtual	boost::shared_array< u_int8_t > Malloc( size_t szSize,size_t* allocated=NULL ); 

	protected:
		virtual	u_int8_t*		Malloc_Internal( size_t szSize,size_t* allocated=NULL );
		virtual	void				Free_Internal( u_int8_t* pPointer );
		static	void				MemItem_Deletor( u_int8_t *pPointer );

	protected:
		CMapMemPoolConfig		m_mapConfig; // 用来记录每种大小内存池的最大容纳个数: 大小－个数
		CMapMemPool					m_mapMemPool; // 大小－（当前已分配的快数，boost内存池）
		boost::recursive_mutex			m_lockPool;
};

#endif
