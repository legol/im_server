#ifndef _OBJECT_POOL_INCLUDED_
#define _OBJECT_POOL_INCLUDED_

#include <boost/smart_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include "mem_pool.h"

#define DEFAULT_OBJECT_COUNT	10000

template< typename TObject >
class TObjectPool;

template< typename TObject >
struct tagObjectItem
{
	TObjectPool< TObject >  *pMemPool; // 分配这块内存的内存池指针
	TObject					pObjPointer[];
};


template< typename TObject >
class TObjectPool
{
	public:
		TObjectPool();
		virtual ~TObjectPool();

		virtual	int32_t Init( u_int32_t uMaxCount );

		virtual	boost::shared_ptr< TObject > Malloc();

	protected:
		virtual	TObject* 	Malloc_Internal();
		virtual	void		Free_Internal( void* pObjPointer );
		static 	void      	ObjItem_Deletor( TObject *pObjPointer );

	public:
		u_int32_t					m_uMaxCount, m_uCurrentCount;
		boost::shared_ptr< CMemPoolBase >	m_pMemPoolBase;
		boost::recursive_mutex          	m_lockPool;
};

	template< typename TObject >
TObjectPool< TObject >::TObjectPool()
{
	m_uMaxCount = DEFAULT_OBJECT_COUNT;
	m_uCurrentCount = 0;

	m_pMemPoolBase = boost::shared_ptr< CMemPoolBase >( new CMemPoolBase( sizeof( tagObjectItem< TObject > ) + sizeof( TObject ) ) );
}

	template< typename TObject >
TObjectPool< TObject >::~TObjectPool()
{
}

	template< typename TObject >
int32_t TObjectPool< TObject >::Init( u_int32_t uMaxCount )
{
	m_uMaxCount = uMaxCount;

	return 0;
}

	template< typename TObject >
TObject* TObjectPool< TObject >::Malloc_Internal()
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );

	tagObjectItem< TObject >	*pObjItem = NULL;

	if( m_uCurrentCount >= m_uMaxCount )
	{
		return NULL;
	}

	pObjItem = ( tagObjectItem< TObject >* )( m_pMemPoolBase->malloc() );
	if( pObjItem )
	{
		m_uCurrentCount++;
		pObjItem->pMemPool = this;
		new ( pObjItem->pObjPointer ) TObject(); // 显式调用构造函数
	}

	return pObjItem->pObjPointer;
}

	template< typename TObject >
void TObjectPool< TObject >::Free_Internal( void *pObjPointer )
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );

	tagObjectItem< TObject >	*pObjItem = NULL;

	if( pObjPointer == NULL )
	{
		return;
	}

	pObjItem = ( tagObjectItem< TObject >* )( ( ( u_int8_t* )pObjPointer ) - sizeof( tagObjectItem< TObject > ) );
	pObjItem->pObjPointer->~TObject(); // 显式调用析构函数
	m_pMemPoolBase->free( pObjItem );
	m_uCurrentCount--;
}

	template< typename TObject >
boost::shared_ptr< TObject > TObjectPool< TObject >::Malloc()
{
	boost::recursive_mutex::scoped_lock	lock( m_lockPool );

	TObject	*pObjPointerRaw = NULL;

	pObjPointerRaw = Malloc_Internal();
	if( pObjPointerRaw != NULL )
	{
		return boost::shared_ptr< TObject >( pObjPointerRaw, ObjItem_Deletor );
	}
	else
	{
		return boost::shared_ptr< TObject >();
	}
}

	template< typename TObject >
void TObjectPool< TObject >::ObjItem_Deletor( TObject *pObjPointer )
{
	tagObjectItem< TObject >  *pObjItem = ( tagObjectItem< TObject >* )( ( ( u_int8_t* )pObjPointer ) - sizeof( tagObjectItem< TObject > ) );
	pObjItem->pMemPool->Free_Internal( pObjPointer );
}


#endif // _OBJECT_POOL_INCLUDED_
