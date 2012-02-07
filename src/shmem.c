#include "shmem.h"

#include "util.h"
#include "zmalloc.h"
#include <string.h>
#include <stdio.h>

struct shmem
{
    char*   name;
    int64_t size;
    int     flags;

    /* platform-specific. */
#if _MSC_VER
#else
    int     fd;
#endif

    void*   mapped;
};

/* forward declarations. */
static bool startup( shmem* s );
static void shutdown( shmem* s );
static void handleError( shmem* s, const char* fmt, ... );
static bool platformStartup( shmem* s );
static void platformShutdown( shmem* s );

/*-----------------------------------------------------------------------------
* Public API definitions.
*----------------------------------------------------------------------------*/

shmem* shmemOpen( const char* name, int64_t size, int flags )
{
    shmem* s = zcalloc( sizeof( shmem ) );
    s->name = strclone( name );
    s->size = size;
    s->flags = flags;
    if ( !startup( s ) )
    {
        shmemClose( s );
        return NULL;
    }
    return s;
}

void shmemClose( shmem* s )
{
    if ( !s )
        return;

    shutdown( s );
    strfree( s->name );
    zfree( s );
}

/*-----------------------------------------------------------------------------
* File-local function definitions.
*----------------------------------------------------------------------------*/

static bool startup( shmem* s )
{
    /* validate inputs. */
    {
        if ( strlen( s->name ) <= 0 )
        {
            handleError( s, "name too short." );
            return false;
        }
    }

    return platformStartup( s );
}

static void shutdown( shmem* s )
{
    platformShutdown( s );
}

static void handleError( shmem* s, const char* fmt, ... )
{
    fprintf( stderr, "shmem('%s') error: ", s->name );
    fprintf( stderr, fmt, (char*)(&fmt + 1) );
    fprintf( stderr, "\n" );
}

/*-----------------------------------------------------------------------------
* Platform-specific function definitions.
*----------------------------------------------------------------------------*/

static bool platformStartup( shmem* s )
{
    return true;
}

static void platformShutdown( shmem* s )
{
}

