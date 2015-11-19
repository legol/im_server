#include "general_data.h"
#include <stdio.h>

CGeneralData::CGeneralData()
{

}

CGeneralData::~CGeneralData()
{
	Clear();
}

int32_t CGeneralData::FieldExists( const std::string &strFieldName, bool *pbExists ) const
{
	if ( pbExists == NULL )
	{
		return -1;
	}

	if ( m_mapData.find( strFieldName ) == m_mapData.end() )
	{
		*pbExists = false;
	}
	else
	{
		*pbExists = true;
	}

	return 0;
}

int32_t CGeneralData::QueryField( const std::string &strFieldName, u_int32_t *puIndex, EGeneralDataType *peType ) const
{
	CMapGeneralData::const_iterator	itr;

	itr = m_mapData.find( strFieldName );
	if ( itr == m_mapData.end() )
	{
		if ( puIndex )
		{
			*puIndex = 0;
		}

		if ( peType )
		{
			*peType = GENERAL_DATA_TYPE_EMPTY;
		}

		return -1;
	}
	else
	{
		*peType = itr->second.eType;
		if ( puIndex )
		{
			*puIndex = 0;
			for ( CVecGeneralDataIndex::const_iterator itrVec = m_vecIndex.begin(); itrVec != m_vecIndex.end(); itrVec++ )
			{
				if ( *itrVec == strFieldName )
				{
					break;
				}
				( *puIndex )++;
			}
		}
		return 0;
	}
	return 0;
}

int32_t CGeneralData::QueryFieldByIndex( u_int32_t uIndex, std::string* pstrFieldName, EGeneralDataType *peType ) const
{
	std::string	strFieldName;
	CMapGeneralData::const_iterator	itr;

	if ( uIndex >= m_vecIndex.size() || pstrFieldName == NULL || peType == NULL )
	{
		return -1;
	}

	strFieldName = m_vecIndex[ uIndex ];
	itr = m_mapData.find( strFieldName );
	if ( itr == m_mapData.end() )
	{
		( *pstrFieldName ).clear();
		*peType = GENERAL_DATA_TYPE_EMPTY;

		return -1;
	}

	*pstrFieldName = strFieldName;
	*peType = itr->second.eType;

	return 0;
}

int32_t CGeneralData::GetFieldCount( u_int32_t* puCount ) const
{
	*puCount = ( u_int32_t )( m_vecIndex.size() );
	return 0;
}

int32_t CGeneralData::RemoveField( const std::string &strFieldName)
{
	CMapGeneralData::iterator	itr;

	itr = m_mapData.find( strFieldName );
	if ( itr == m_mapData.end() )
	{
		return -1;
	}

	ClearItem( itr->second );
	m_mapData.erase( itr );
	for ( CVecGeneralDataIndex::iterator itrVec = m_vecIndex.begin(); itrVec != m_vecIndex.end(); itrVec++ )
	{
		if ( *itrVec == strFieldName )
		{
			m_vecIndex.erase( itrVec );
			break;
		}
	}

	return 0;
}

int32_t	CGeneralData::GetItem( const std::string &strFieldName, tagItem *pvtData ) const
{
	CMapGeneralData::const_iterator	itr;

	if ( pvtData == NULL )
	{
		return -1;
	}

	itr = m_mapData.find( strFieldName );
	if ( itr == m_mapData.end() )
	{
		pvtData->eType = GENERAL_DATA_TYPE_EMPTY;
		pvtData->pvData = NULL;
		return -1;
	}
	else
	{
		pvtData->eType = itr->second.eType;
		pvtData->pvData = itr->second.pvData;
		return 0;
	}
}


#define DEL_ITEM( enumType, valueType ) \
	case enumType: \
{ \
	valueType	*pValue = ( valueType* )( objItem.pvData ); \
	delete pValue; \
} \
break; \

int32_t CGeneralData::ClearItem( tagItem &objItem )
{
	switch( objItem.eType )
	{
		case GENERAL_DATA_TYPE_EMPTY:
			{
				// 不应该进入这里
				return -1;
			}
			break;

			DEL_ITEM( GENERAL_DATA_TYPE_INT8, int8_t )
				DEL_ITEM( GENERAL_DATA_TYPE_UINT8, u_int8_t )
				DEL_ITEM( GENERAL_DATA_TYPE_INT16, int16_t )
				DEL_ITEM( GENERAL_DATA_TYPE_UINT16, u_int8_t )
				DEL_ITEM( GENERAL_DATA_TYPE_INT32, int32_t )
				DEL_ITEM( GENERAL_DATA_TYPE_UINT32, u_int32_t )
				DEL_ITEM( GENERAL_DATA_TYPE_INT64, int64_t )
				DEL_ITEM( GENERAL_DATA_TYPE_UINT64, u_int64_t )
				DEL_ITEM( GENERAL_DATA_TYPE_FLOAT, float )
				DEL_ITEM( GENERAL_DATA_TYPE_DOUBLE, double )
				DEL_ITEM( GENERAL_DATA_TYPE_BOOL, bool )
				DEL_ITEM( GENERAL_DATA_TYPE_POINTER, void* )
				DEL_ITEM( GENERAL_DATA_TYPE_STR, std::string )
				DEL_ITEM( GENERAL_DATA_TYPE_CHAR, char )
				DEL_ITEM( GENERAL_DATA_TYPE_WCHAR, wchar_t )

		case GENERAL_DATA_TYPE_DATA:
				{
					if( objItem.pGeneralData.get() != NULL )
					{
						objItem.pGeneralData.reset();
					}
				}
				break;

		default:
				{
					return -1;
				}
				break;

	}
	return 0;
}

int32_t CGeneralData::Clear()
{
	for( CMapGeneralData::iterator itr = m_mapData.begin(); itr != m_mapData.end(); itr++ )
	{
		ClearItem( itr->second );
	}

	m_mapData.clear();
	m_vecIndex.clear();
	return 0;
}

// 简单类型的 set
#define IMPL_SET( funcName, valueType, enumType ) \
	int32_t CGeneralData::funcName( const std::string &strFieldName, valueType Value ) \
{ \
	tagItem	objItem, objNewItem; \
	GetItem( strFieldName, &objItem ); \
	if( objItem.eType != GENERAL_DATA_TYPE_EMPTY && ( objItem.eType != enumType ) ) \
	{ \
		return -1; \
	} \
	objNewItem.eType = enumType; \
	objNewItem.pvData = new valueType; \
	*( valueType* )( objNewItem.pvData ) = Value; \
	m_mapData[ strFieldName ] = objNewItem; \
	m_vecIndex.push_back( strFieldName ); \
	return 0; \
} \

// 简单类型的 get
#define IMPL_GET( funcName, valueType, enumType ) \
	int32_t	CGeneralData::funcName( const std::string &strFieldName, valueType *pValue ) const \
{ \
	tagItem		objItem; \
	if( pValue == NULL ) \
	{ \
		return -1; \
	} \
	if( GetItem( strFieldName, &objItem ) == 0 && objItem.eType == enumType ) \
	{ \
		*pValue = *( valueType* )( objItem.pvData ); \
		return 0; \
	} \
	return -1; \
} \


	IMPL_SET( SetInt8, int8_t, GENERAL_DATA_TYPE_INT8 )
IMPL_GET( GetInt8, int8_t, GENERAL_DATA_TYPE_INT8 )

	IMPL_SET( SetUint8, u_int8_t, GENERAL_DATA_TYPE_UINT8 )
IMPL_GET( GetUint8, u_int8_t, GENERAL_DATA_TYPE_UINT8 )

	IMPL_SET( SetInt16, int16_t, GENERAL_DATA_TYPE_INT16 )
IMPL_GET( GetInt16, int16_t, GENERAL_DATA_TYPE_INT16 )

	IMPL_SET( SetUint16, u_int16_t, GENERAL_DATA_TYPE_UINT16 )
IMPL_GET( GetUint16, u_int16_t, GENERAL_DATA_TYPE_UINT16 )

	IMPL_SET( SetInt32, int32_t, GENERAL_DATA_TYPE_INT32 )
IMPL_GET( GetInt32, int32_t, GENERAL_DATA_TYPE_INT32 )

	IMPL_SET( SetUint32, u_int32_t, GENERAL_DATA_TYPE_UINT32 )
IMPL_GET( GetUint32, u_int32_t, GENERAL_DATA_TYPE_UINT32 )

	IMPL_SET( SetInt64, int64_t, GENERAL_DATA_TYPE_INT64 )
IMPL_GET( GetInt64, int64_t, GENERAL_DATA_TYPE_INT64 )

	IMPL_SET( SetUint64, u_int64_t, GENERAL_DATA_TYPE_UINT64 )
IMPL_GET( GetUint64, u_int64_t, GENERAL_DATA_TYPE_UINT64 )

	IMPL_SET( SetFloat, float, GENERAL_DATA_TYPE_FLOAT )
IMPL_GET( GetFloat, float, GENERAL_DATA_TYPE_FLOAT )

	IMPL_SET( SetDouble, double, GENERAL_DATA_TYPE_DOUBLE )
IMPL_GET( GetDouble, double, GENERAL_DATA_TYPE_DOUBLE )

	IMPL_SET( SetPointer, void*, GENERAL_DATA_TYPE_POINTER )
IMPL_GET( GetPointer, void*, GENERAL_DATA_TYPE_POINTER )

	IMPL_SET( SetBool, bool, GENERAL_DATA_TYPE_BOOL )
IMPL_GET( GetBool, bool, GENERAL_DATA_TYPE_BOOL )

	IMPL_SET( SetChar, char, GENERAL_DATA_TYPE_CHAR )
IMPL_GET( GetChar, char, GENERAL_DATA_TYPE_CHAR )

	IMPL_SET( SetWChar, wchar_t, GENERAL_DATA_TYPE_WCHAR )
IMPL_GET( GetWChar, wchar_t, GENERAL_DATA_TYPE_WCHAR )



	// 字符串参数不希望拷贝传递，所以不使用 IMPL_SET 宏
int32_t CGeneralData::SetStr( const std::string &strFieldName, const std::string &strValue)
{ 
	tagItem	objItem, objNewItem; 

	GetItem( strFieldName, &objItem ); 
	if( objItem.eType != GENERAL_DATA_TYPE_EMPTY && ( objItem.eType != GENERAL_DATA_TYPE_STR ) ) 
	{ 
		return -1; 
	} 

	objNewItem.eType = GENERAL_DATA_TYPE_STR; 
	objNewItem.pvData = new std::string; 
	*( std::string* )( objNewItem.pvData ) = strValue; 
	m_mapData[ strFieldName ] = objNewItem; 
	m_vecIndex.push_back( strFieldName ); 

	return 0; 
}

IMPL_GET( GetStr, std::string, GENERAL_DATA_TYPE_STR )

int32_t CGeneralData::SetData( const std::string &strFieldName, boost::shared_ptr< CGeneralData > pGeneralData )
{
	tagItem	objItem, objNewItem;

	GetItem( strFieldName, &objItem );
	if( objItem.eType != GENERAL_DATA_TYPE_EMPTY && ( objItem.eType != GENERAL_DATA_TYPE_DATA ) )
	{
		return -1;
	}

	objNewItem.eType = GENERAL_DATA_TYPE_DATA;
	objNewItem.pvData = NULL;
	objNewItem.pGeneralData = pGeneralData;
	m_mapData[ strFieldName ] = objNewItem;
	m_vecIndex.push_back( strFieldName );

	return 0;
}

int32_t CGeneralData::GetData( const std::string &strFieldName, boost::shared_ptr< CGeneralData > *ppGeneralData )
{
	tagItem		objItem;
	if( ppGeneralData == NULL )
	{
		return -1;
	}

	if( GetItem( strFieldName, &objItem ) == 0 && objItem.eType == GENERAL_DATA_TYPE_DATA )
	{
		*ppGeneralData = objItem.pGeneralData;
		return 0;
	}

	return -1;
}

