#ifndef _MISC_INCLUDED_
#define _MISC_INCLUDED_

#include <sys/types.h>
#include <string>

int32_t strlwr( char *pstrInOut, u_int32_t uLen );
int32_t	strupr( char *pstrInOut, u_int32_t uLen );

int utf8_2_gbk(const char* src,size_t src_len,char *dst,size_t dst_len,size_t* out_len);
int gbk_2_utf8(const char* src,size_t src_len,char *dst,size_t dst_len,size_t* out_len);

int32_t HexLog( const u_int8_t* pchBuff, u_int32_t uLen, std::string *pstrLog );
int32_t HexLog2( const u_int8_t* pchBuff, u_int32_t uLen, std::string *pstrLog );

int32_t PathFromFileURL( const char* strFileURL, std::string *pstrPath );
int32_t FileNameFromFileURL( const char *strFileURL, std::string *pstrFileName );

#endif
