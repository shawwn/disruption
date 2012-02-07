#include "util.h"

#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "zmalloc.h"

void strfree( char* str )
{
    zfree( str );
}

char* strclone( const char* str )
{
    size_t len;
    char* result;

    len = strlen( str );
    result = zmalloc( len + 1 );
    strcpy( result, str );
    return result;
}

char* vstrformat( const char* fmt, va_list ap )
{
    size_t len;
    char* result;

    /* determine the length of the resultant string. */
    {
        int ret;
        va_list tmp = ap;
        ret = vsnprintf( NULL, 0, fmt, tmp );
        assert( ret >= 0 );
        len = (size_t)ret + 1;
    }

    /* allocate space to store it. */
    result = zmalloc( len + 1 );
    result[ len ] = '\0';

    /* build it. */
    vsnprintf( result, len, fmt, ap );

    /* return it. */
    return result;
}

char* strformat( const char* fmt, ... )
{
    char* result;
    va_list ap;

    va_start( ap, fmt );
    result = vstrformat( fmt, ap );
    va_end( ap );
    return result;
}


