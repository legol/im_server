#ifndef _GENERAL_DATA_INCLUDED_
#define _GENERAL_DATA_INCLUDED_

// author: chenjie
// todo:这个类不是线程安全的
// todo:使用内存池

/*
   how to use:

   boost::shared_ptr< CGeneralData >	pGeneralData = CGeneralData::CreateInstance();
   pGeneralData->SetInt8( "abc", 123 );

   int8_t	chValue;
   pGeneralData->GetInt8( "abc", &chValue );

*/
#include <map>
#include <vector>
#include <string>
#include <sys/types.h>
#include <boost/smart_ptr.hpp>


enum EGeneralDataType
{
	GENERAL_DATA_TYPE_EMPTY,
	GENERAL_DATA_TYPE_BOOL,
	GENERAL_DATA_TYPE_CHAR,
	GENERAL_DATA_TYPE_WCHAR,
	GENERAL_DATA_TYPE_STR,
	GENERAL_DATA_TYPE_POINTER,
	GENERAL_DATA_TYPE_INT8,
	GENERAL_DATA_TYPE_UINT8,
	GENERAL_DATA_TYPE_INT16,
	GENERAL_DATA_TYPE_UINT16,
	GENERAL_DATA_TYPE_INT32,
	GENERAL_DATA_TYPE_UINT32,
	GENERAL_DATA_TYPE_INT64,
	GENERAL_DATA_TYPE_UINT64,
	GENERAL_DATA_TYPE_FLOAT,
	GENERAL_DATA_TYPE_DOUBLE,
	GENERAL_DATA_TYPE_BUFFER,
	GENERAL_DATA_TYPE_DATA,
	GENERAL_DATA_TYPE_ARRAY,
};


class CGeneralData;

struct tagItem
{
	EGeneralDataType 			eType;
	void						*pvData;
	boost::shared_ptr< CGeneralData >	pGeneralData;
};

#define GENERAL_DATA_DOC_DATA	"Data"
#define GENERAL_DATA_DOC_ARRAY	"Array"
#define GENERAL_DATA_DOC_ITEM	"Item"

#define GENERAL_DATA_DOC_VERSION	"version"
#define GENERAL_DATA_DOC_TYPE		"type"
#define GENERAL_DATA_DOC_NAME		"name"
#define GENERAL_DATA_DOC_VALUE		"value"

typedef	std::map< std::string, tagItem >			CMapGeneralData;
typedef	std::vector< std::string >					CVecGeneralDataIndex;

class CGeneralData
{
	public:
		static  boost::shared_ptr< CGeneralData > CreateInstance()
		{
			return boost::shared_ptr< CGeneralData >( new (std::nothrow) CGeneralData() );
		}

		// 使用私有来禁止掉这些行为，只允许通过 boost::shared_ptr 来访问
	private:
		CGeneralData();
		CGeneralData( const CGeneralData& right );
		const CGeneralData& operator=( const CGeneralData& right );

	public:						
		virtual	~CGeneralData();

	public:
		int32_t QueryFieldByIndex( u_int32_t uIndex, std::string* pstrFieldName, EGeneralDataType *peType ) const;
		int32_t QueryField( const std::string &strFieldName, u_int32_t *puIndex, EGeneralDataType *peType ) const;
		int32_t FieldExists( const std::string &strFieldName, bool *pbExists ) const;
		int32_t GetFieldCount( u_int32_t* puCount) const;
		int32_t RemoveField( const std::string &strFieldName );
		int32_t GetItem( const std::string &strFieldName, tagItem *pvtData ) const; // 只是简单的将 map 中的 tagItem 取出，并不会复制 tagItem.pvData
		int32_t	Clear();
		int32_t	ClearItem( tagItem &objItem );

		int32_t SetInt8( const std::string &strFieldName, int8_t n8Value );
		int32_t	GetInt8( const std::string &strFieldName, int8_t *pn8Value ) const;

		int32_t SetUint8( const std::string &strFieldName, u_int8_t u8Value );
		int32_t	GetUint8( const std::string &strFieldName, u_int8_t *pu8Value ) const;

		int32_t SetInt16( const std::string &strFieldName, int16_t n16Value );
		int32_t	GetInt16( const std::string &strFieldName, int16_t *pn16Value ) const;

		int32_t SetUint16( const std::string &strFieldName, u_int16_t u16Value );
		int32_t	GetUint16( const std::string &strFieldName, u_int16_t *pu16Value ) const;

		int32_t SetInt32( const std::string &strFieldName, int32_t n32Value );
		int32_t	GetInt32( const std::string &strFieldName, int32_t *pn32Value ) const;

		int32_t SetUint32( const std::string &strFieldName, u_int32_t u32Value );
		int32_t	GetUint32( const std::string &strFieldName, u_int32_t *pu32Value ) const;

		int32_t SetInt64( const std::string &strFieldName, int64_t n64Value );
		int32_t	GetInt64( const std::string &strFieldName, int64_t *pn64Value ) const;

		int32_t SetUint64( const std::string &strFieldName, u_int64_t u64Value );
		int32_t	GetUint64( const std::string &strFieldName, u_int64_t *pu64Value ) const;

		int32_t SetDouble( const std::string &strFieldName, double dblValue );
		int32_t GetDouble( const std::string &strFieldName, double *pdblValue ) const;

		int32_t SetFloat( const std::string &strFieldName, float fltValue);
		int32_t GetFloat( const std::string &strFieldName, float *pfltValue) const;

		int32_t SetBool( const std::string &strFieldName,  bool bValue );
		int32_t GetBool( const std::string &strFieldName,  bool *pbValue ) const;

		int32_t SetChar( const std::string &strFieldName,  char chValue );
		int32_t GetChar( const std::string &strFieldName,  char *pchValue ) const;

		int32_t SetWChar( const std::string &strFieldName, wchar_t wchValue );
		int32_t GetWChar( const std::string &strFieldName, wchar_t *pwchValue ) const;

		int32_t SetStr( const std::string &strFieldName, const std::string &strValue) ;
		int32_t GetStr( const std::string &strFieldName,  std::string *pstrValue ) const;

		int32_t SetPointer( const std::string &strFieldName, void *pValue );
		int32_t GetPointer( const std::string &strFieldName, void **ppvValue ) const;

		int32_t	SetData( const std::string &strFieldName, boost::shared_ptr< CGeneralData > pGeneralData );
		int32_t	GetData( const std::string &strFieldName, boost::shared_ptr< CGeneralData > *ppGeneralData );

		//int32_t SetBuffer( const std::string &strFieldName, CBuffer* pBuf );
		//int32_t GetBuffer( const std::string &strFieldName, CBuffer** ppBuf ) const;

		//int32_t SetArray( const std::string &strFieldName, CGeneralArray* pArray );
		//int32_t GetArray( const std::string &strFieldName, CGeneralArray** ppArray ) const;

	protected:
		CMapGeneralData			m_mapData;
		CVecGeneralDataIndex	m_vecIndex; 
};

#endif // _GENERAL_DATA_INCLUDED_
