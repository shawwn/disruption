#include "zmalloc.h"

#include <stdlib.h>

void* zmalloc( size_t size )
{
    return malloc( size );
}

void* zcalloc( size_t size )
{
    return calloc( 1, size );
}

void zfree( void* ptr )
{
    free( ptr );
}

