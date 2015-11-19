#include "misc.h"
#include <ctype.h>
#include <stdio.h>
#include <iconv.h>
#include <string.h>

int32_t strlwr( char *pstrInOut, u_int32_t uLen )
{
	if( pstrInOut == NULL )
	{
		return 0;
	}

	for( u_int32_t i = 0; i < uLen; i++ )
	{
		*( pstrInOut + i ) = tolower( *( pstrInOut + i ) );
	}

	return 0;
}

int32_t	strupr( char *pstrInOut, u_int32_t uLen )
{
	if( pstrInOut == NULL )
	{
		return 0;
	}

	for( u_int32_t i = 0; i < uLen; i++ )
	{
		*( pstrInOut + i ) = toupper( *( pstrInOut + i ) );
	}

	return 0;
}

int utf8_2_gbk(const char* src,size_t src_len,char *dst,size_t dst_len,size_t* out_len)
{
	if(!src || !dst || !out_len)
	{
		return -1;
	}
	*out_len = 0;
	iconv_t conv_handle = iconv_open("gbk","utf-8");
	if((iconv_t)-1 == conv_handle)
	{
		return -1;
	}
	char** inbuf = (char**)&src;
	size_t inbytesleft = src_len;
	char** outbuf = &dst;
	size_t outbytesleft = dst_len;
	size_t ret = iconv(conv_handle,inbuf,&inbytesleft,outbuf,&outbytesleft);
	iconv_close(conv_handle);
	*out_len = dst_len - outbytesleft ;
	if(ret != (size_t)(-1) )
	{
		return 0;
	}

	return -1;
}

int gbk_2_utf8(const char* src,size_t src_len,char *dst,size_t dst_len,size_t* out_len)
{
	if(!src || !dst || !out_len)
	{
		return -1;
	}
	*out_len = 0;
	iconv_t conv_handle = iconv_open("utf-8","gbk");
	if((iconv_t)-1 == conv_handle)
	{
		return -1;
	}
	char** inbuf = (char**)&src;
	size_t inbytesleft = src_len;
	char** outbuf = &dst;
	size_t outbytesleft = dst_len;
	size_t ret = iconv(conv_handle,inbuf,&inbytesleft,outbuf,&outbytesleft);
	iconv_close(conv_handle);
	*out_len = dst_len - outbytesleft ;
	if(ret != (size_t)(-1) )
	{
		return 0;
	}

	return -1;
}

#define	PRINTABLE_CHAR(c) ( ( (c) >= ' ' && (c) <= '~' ) ? (c) : '*' )

int32_t HexLog( const u_int8_t* pchBuff, u_int32_t uLen, std::string *pstrLog )
{
	u_int32_t	uCurrentPos, uLineNum, uBlankCount;

	char		strPartA[ 100 ] = { 0 }; // line number
	char		strPartB[ 100 ] = { 0 }; // first 8 bytes
	char		strPartC[ 100 ] = { 0 }; // second 8 bytes
	char		strPartD[ 100 ] = { 0 }; // first 8 chars
	char		strPartE[ 100 ] = { 0 }; // second 8 chars

	char		strTmp[ 100 ] = { 0 };

	bool		bFinished = false;

	if( pchBuff == NULL || uLen == 0 )
	{
		( *pstrLog ) += " { } ";
		return 0;
	}

	// format: partA: partB -- partC  |  partD -- partE\n
	( *pstrLog ) += ( " { \r\n" );

	uCurrentPos = 0;
	uLineNum = 0;
	while( uCurrentPos < uLen )
	{

		memset( strPartA, 0, sizeof( strPartA ) );
		memset( strPartB, 0, sizeof( strPartB ) );
		memset( strPartC, 0, sizeof( strPartC ) );
		memset( strPartD, 0, sizeof( strPartD ) );
		memset( strPartE, 0, sizeof( strPartE ) );
		bFinished = false;

		snprintf( strPartA, 99, "%06X", uLineNum );

		if( uCurrentPos + 8 <= uLen )
		{
			snprintf( strPartB, 99, "%02X  %02X  %02X  %02X  %02X  %02X  %02X  %02X", 
					*( pchBuff + uCurrentPos ),
					*( pchBuff + uCurrentPos + 1 ),
					*( pchBuff + uCurrentPos + 2 ),
					*( pchBuff + uCurrentPos + 3 ),
					*( pchBuff + uCurrentPos + 4 ),
					*( pchBuff + uCurrentPos + 5 ),
					*( pchBuff + uCurrentPos + 6 ),
					*( pchBuff + uCurrentPos + 7 ) );

			snprintf( strPartD, 99, "%c %c %c %c %c %c %c %c",
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 1 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 2 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 3 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 4 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 5 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 6 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 7 ) )
					);

			uCurrentPos += 8;
		}
		else
		{
			uBlankCount = uCurrentPos + 8 - uLen;

			if( uCurrentPos < uLen )
			{
				snprintf( strTmp, 99, "%02X", *( u_int8_t* )( pchBuff + uCurrentPos ) );
				strcat( strPartB, strTmp );

				snprintf( strTmp, 99, "%c", PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos ) ) );
				strcat( strPartD, strTmp );

				uCurrentPos++;
			}

			for ( ; uCurrentPos < uLen; uCurrentPos++ )
			{
				snprintf( strTmp, 99, "  %02X", *( u_int8_t* )( pchBuff + uCurrentPos ) );
				strcat( strPartB, strTmp );

				snprintf( strTmp, 99, " %c", PRINTABLE_CHAR( *( u_int8_t* )( pchBuff + uCurrentPos ) ) );
				strcat( strPartD, strTmp );
			}

			if( uBlankCount == 8 )
			{
				snprintf( strTmp, 99, "  " );
				strcat( strPartB, strTmp );

				snprintf( strTmp, 99, " " );
				strcat( strPartD, strTmp );

				uBlankCount--;
			}

			for ( u_int32_t uTmp = 0; uTmp < uBlankCount; uTmp++ )
			{
				snprintf( strTmp, 99, "    " );
				strcat( strPartB, strTmp );

				snprintf( strTmp, 99, "  " );
				strcat( strPartD, strTmp );
			}

			bFinished = true;
		}

		if( uCurrentPos + 8 <= uLen )
		{
			snprintf( strPartC, 99, "%02X  %02X  %02X  %02X  %02X  %02X  %02X  %02X", 
					*( pchBuff + uCurrentPos ),
					*( pchBuff + uCurrentPos + 1 ),
					*( pchBuff + uCurrentPos + 2 ),
					*( pchBuff + uCurrentPos + 3 ),
					*( pchBuff + uCurrentPos + 4 ),
					*( pchBuff + uCurrentPos + 5 ),
					*( pchBuff + uCurrentPos + 6 ),
					*( pchBuff + uCurrentPos + 7 ) );

			snprintf( strPartE, 99, "%c %c %c %c %c %c %c %c",
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 1 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 2 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 3 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 4 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 5 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 6 ) ),
					PRINTABLE_CHAR( *( char* )( pchBuff + uCurrentPos + 7 ) )
					);

			uCurrentPos += 8;
		}
		else
		{
			uBlankCount = uCurrentPos + 8 - uLen;

			if( uCurrentPos < uLen )
			{
				snprintf( strTmp, 99, "%02X", *( u_int8_t* )( pchBuff + uCurrentPos ) );
				strcat( strPartC, strTmp );

				snprintf( strTmp, 99, "%c", PRINTABLE_CHAR( *( u_int8_t* )( pchBuff + uCurrentPos ) ) );
				strcat( strPartE, strTmp );

				uCurrentPos++;
			}

			for ( ; uCurrentPos < uLen; uCurrentPos++ )
			{
				snprintf( strTmp, 99, "  %02X", *( u_int8_t* )( pchBuff + uCurrentPos ) );
				strcat( strPartC, strTmp );

				snprintf( strTmp, 99, " %c", PRINTABLE_CHAR( *( u_int8_t* )( pchBuff + uCurrentPos ) ) );
				strcat( strPartE, strTmp );
			}

			if( uBlankCount == 8 )
			{
				snprintf( strTmp, 99, "  " );
				strcat( strPartC, strTmp );

				snprintf( strTmp, 99, " " );
				strcat( strPartE, strTmp );

				uBlankCount--;
			}

			for ( u_int32_t uTmp = 0; uTmp < uBlankCount; uTmp++ )
			{
				snprintf( strTmp, 99, "    " );
				strcat( strPartC, strTmp );

				snprintf( strTmp, 99, "  " );
				strcat( strPartE, strTmp );
			}

			bFinished = true;
		}

		( *pstrLog ) += ( std::string ( strPartA ) + std::string( ": " ) + 
				std::string ( strPartB ) + std::string( " -- " ) + std::string( strPartC ) + std::string( "  |  " ) + 
				std::string( strPartD ) + std::string( " -- ") + std::string( strPartE ) + std::string( "\r\n" ) );
		uLineNum++;

		if( bFinished )
		{
			break;
		}

	}

	( *pstrLog ) += ( " } \r\n" );

	return 0;
}

int32_t HexLog2( const u_int8_t* pchBuff, u_int32_t uLen, std::string *pstrLog )
{
	u_int32_t   uCurrentPos;
	char        *strHex, strTmp[ 100 ];

	if( pstrLog == NULL )
	{
		return -1;
	}

	if( pchBuff == NULL || uLen == 0 )
	{
		( *pstrLog ) += " { } ";
		return 0;
	}

	strHex = new char[ uLen * 4 + 1 ];
	memset( strHex, 0, uLen * 4 + 1 );

	uCurrentPos = 0;
	if( uCurrentPos < uLen )
	{
		snprintf( strTmp, 99, " { %02X", *( u_int8_t* )( pchBuff + uCurrentPos ) );
		strcat( strHex, strTmp );

		uCurrentPos++;
	}

	for ( ; uCurrentPos < uLen; uCurrentPos++ )
	{
		snprintf( strTmp, 99, "  %02X", *( u_int8_t* )( pchBuff + uCurrentPos ) );
		strcat( strHex, strTmp );
	}

	( *pstrLog ) += strHex;

	( *pstrLog ) += " } ";

	delete strHex;
	return 0;
}

int32_t PathFromFileURL( const char* strFileURL, std::string *pstrPath )
{
	int32_t		iFileURLLength = 0;

	if ( strFileURL == NULL || pstrPath == NULL )
	{
		return -1;
	}

	iFileURLLength = strlen( strFileURL );
	while (iFileURLLength > 0)
	{
		if ( strFileURL[iFileURLLength - 1] == '/' )
		{
			break;
		}

		--iFileURLLength;
	}

	*pstrPath = std::string( strFileURL, 0, iFileURLLength );

	return 0;
}

int32_t FileNameFromFileURL( const char *strFileURL, std::string *pstrFileName )
{
	return -1;
}

