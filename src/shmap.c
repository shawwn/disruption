#include "shmap.h"

#include "util.h"
#include "zmalloc.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

struct shmap
{
    char*   name;
    char*   mem;
    int64_t size;
    int64_t itemSize;
};

/* forward declarations. */
static bool startup( shmap* s );
static void shutdown( shmap* s );
static void handleError( shmap* s, const char* fmt, ... );

/*-----------------------------------------------------------------------------
* Public API definitions.
*----------------------------------------------------------------------------*/

shmap* shmapCreate( void* mem, int64_t memSize, size_t itemSize, const char* debugName, ... )
{
    shmap* s = zcalloc( sizeof( shmap ) );
    s->name = strformat( debugName, (char*)(&debugName + 1) );
    s->mem = (char*)mem;
    s->size = memSize;
    s->itemSize = itemSize;
    if ( !startup( s ) )
    {
        shmapRelease( s );
        return NULL;
    }
    return s;
}

void shmapRelease( shmap* s )
{
    if ( !s )
        return;

    strfree( s->name );
    shutdown( s );
    zfree( s );
}

/*-----------------------------------------------------------------------------
* File-local function definitions.
*----------------------------------------------------------------------------*/

static bool startup( shmap* s )
{
    /* validate inputs. */
    {
        /* too short? */
        if ( s->size <= 0 )
        {
            handleError( s, "memory not large enough." );
            return false;
        }

        /* check for slashes in the name. */
        if ( !s->mem )
        {
            handleError( s, "invalid memory." );
            return false;
        }
    }

    return true;
}

static void shutdown( shmap* s )
{
    (void)s;
}

static void handleError( shmap* s, const char* fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    fprintf( stderr, "shmap('%s') error: ", s->name );
    vfprintf( stderr, fmt, ap );
    fprintf( stderr, "\n" );
    va_end( ap );
}

