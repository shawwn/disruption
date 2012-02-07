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

char* strclone( char* str )
{
    size_t len;
    char* result;

    len = strlen( str );
    result = zmalloc( len + 1 );
    strcpy( result, str );
    return result;
}

char* strformat( char* fmt, ... )
{
    size_t len;
    char* result;

    /* determine the length of the resultant string. */
    {
        int ret;
        ret = vsnprintf( NULL, 0, fmt, (char*)(&fmt + 1) );
        assert( ret >= 0 );
        len = (size_t)ret;
    }

    /* allocate space to store it. */
    result = zmalloc( len + 1 );
    result[ len ] = '\0';

    /* build it. */
    vsnprintf( result, len + 1, fmt, (char*)(&fmt + 1) );

    /* return it. */
    return result;
}


