#include <stdio.h>
#include <iostream>
#include <string>
#include "pcre.h"

int main( int32_t argc, char* argv[] )
{
	// pcre 必需要自己编译，系统带的无法连接
	pcre 			*re;
	const char 		*error;
	int				erroffset;
	const u_int32_t	c_uMaxMatchCount = 1000;
	int 			ovector[ 3 * c_uMaxMatchCount ];
	int 			rc;

	std::string		strRegex, strSubject;

	if( argc != 3 )
	{
		std::cout << "usage: pcre_regex_tester regex subject" << std::endl;
		return -1;
	}

	strRegex.assign( argv[ 1 ], strlen( argv[ 1 ] ) );
	if( strRegex.length() >= 2 )
	{
		if( *( strRegex.begin() ) == '"' && *( strRegex.rbegin() ) == '"' )
		{
			strRegex.erase( 0, 1 );
			strRegex.erase( strRegex.length() - 1, 1 );
		}
	}
	std::cout << "regex=" << strRegex << std::endl;

	strSubject.assign( argv[ 2 ], strlen( argv[ 2 ] ) );
	if( strSubject.length() >= 2 )
	{
		if( *( strSubject.begin() ) == '"' && *( strSubject.rbegin() ) == '"' )
		{
			strSubject.erase( 0, 1 );
			strSubject.erase( strSubject.length() - 1, 1 );
		}
	}
	std::cout << "subject=" << strSubject << std::endl;

	/* Compile the regular expression in the first argument */
	re = pcre_compile(
			strRegex.c_str(),     /* the pattern */
			PCRE_CASELESS,           /* options */
			&error,      /* for error message */
			&erroffset,  /* for error offset */
			NULL);       /* use default character tables */

	/* Compilation failed: print the error message and exit */
	if (re == NULL)
	{
		std::cout << "regex format wrong:" << error << std::endl;
		return -1;
	}

	u_int32_t		uTotalMatch;
	u_int32_t		uStart;
	const char		*pstrSubStringStart;
	u_int32_t		uSubStringLen;
	std::string		strSubString;

	uTotalMatch = 0;
	uStart = 0;
	while( uStart < strSubject.length() &&
			( rc = pcre_exec( re,          /* the compiled pattern */
							NULL,        /* we didn't study the pattern */
							strSubject.c_str(), // subject
							strSubject.length(), // subject len
							uStart,           /* start at offset 0 in the subject */
							0,           /* default options */
							ovector,     /* vector for substring information */
							3 * c_uMaxMatchCount ) ) > 0 )  /* number of elements in the vector */
	{
		uTotalMatch++;

		std::cout << "match count:" << rc << std::endl;
		for ( int32_t i = 0; i < rc; i++)
		{
			pstrSubStringStart = strSubject.c_str() + ovector[2*i];
			uSubStringLen = ovector[2*i+1] - ovector[2*i];
			strSubString.assign( pstrSubStringStart, uSubStringLen );
	
			std::cout << strSubString << std::endl;
		}
		std::cout << std::endl;
		uStart = ovector[ 1 ];
	}

	std::cout << "total match:" << uTotalMatch << std::endl;

	return 0;
}
