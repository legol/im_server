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
 *  @CMapMemPoolConfig		�ڴ���С������
 */
typedef std::map< size_t, u_int32_t >		CMapMemPoolConfig;

/**
 *  @CMapMemPool		�ڴ���С������ǰ��������Ӧ��boost�ڴ�أ�
 */
typedef std::map< size_t, tagMemPool >	CMapMemPool; 

class CMemPool;

struct tagMemItem
{
	CMemPool	*pMemPool; // ��������ڴ���ڴ��ָ��
	size_t		szSize; // ����ڴ�Ĵ�С,�� pPointerָ����ڴ���С
	u_int8_t	pPointer[]; // �ڴ��
};

class CMemPool
{
	public:
		CMemPool();
		virtual ~CMemPool();

		virtual	int32_t		Init( const CMapMemPoolConfig &mapConfig ); // �ú���ֻ��������һ��
		virtual	int32_t		Init( const std::string &strMemPoolCfg ); // ����������ʽ���ַ���( size, count ), ( size, count )...
		virtual	int32_t 	Destroy();
		void							Dump();

		virtual	boost::shared_array< u_int8_t > Malloc( size_t szSize,size_t* allocated=NULL ); 

	protected:
		virtual	u_int8_t*		Malloc_Internal( size_t szSize,size_t* allocated=NULL );
		virtual	void				Free_Internal( u_int8_t* pPointer );
		static	void				MemItem_Deletor( u_int8_t *pPointer );

	protected:
		CMapMemPoolConfig		m_mapConfig; // ������¼ÿ�ִ�С�ڴ�ص�������ɸ���: ��С������
		CMapMemPool					m_mapMemPool; // ��С������ǰ�ѷ���Ŀ�����boost�ڴ�أ�
		boost::recursive_mutex			m_lockPool;
};

#endif
