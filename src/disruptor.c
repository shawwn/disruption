#include "disruptor.h"

#include "util.h"
#include "zmalloc.h"
#include <stdio.h>
#include <string.h>

struct disruptor
{
    char* address;
    char* username;
};

/* forward declarations. */
static bool initialize( disruptor* d );
static void shutdown( disruptor* d );
static void handleError( disruptor* d, const char* fmt, ... );

/* constants. */
#define MAX_ADDRESS_LENGTH      32
#define MAX_USERNAME_LENGTH     32

/*-----------------------------------------------------------------------------
* Public API definitions.
*----------------------------------------------------------------------------*/

disruptor* disruptorCreate( const char* address, const char* username )
{
    disruptor* d = zcalloc( sizeof(disruptor) );
    d->address = strclone( address );
    d->username = strclone( username );
    if ( !initialize( d ) )
    {
        disruptorRelease( d );
        return NULL;
    }
    return d;
}


void disruptorRelease( disruptor* d )
{
    if ( !d )
        return;

    shutdown( d );
    strfree( d->address );
    strfree( d->username );
    zfree( d );
}

/*-----------------------------------------------------------------------------
* File-local function definitions.
*----------------------------------------------------------------------------*/

static bool isStringValid( const char* str, size_t minSize, size_t maxSize )
{
    size_t len;
    
    len = strlen( str );
    if ( len < minSize )
        return false;
    if ( len > maxSize )
        return false;
    return true;
}

static bool initialize( disruptor* d )
{
    /* validate inputs. */
    {
        if ( !isStringValid( d->address, 1, MAX_ADDRESS_LENGTH ) )
        {
            handleError( d, "invalid length for string '%s'", d->address );
            return false;
        }

        if ( !isStringValid( d->username, 1, MAX_USERNAME_LENGTH ) )
        {
            handleError( d, "invalid length for string '%s'", d->username );
            return false;
        }
    }

    return true;
}

static void shutdown( disruptor* d )
{
    (void)d;
}

static void handleError( disruptor* d, const char* fmt, ... )
{
    fprintf( stderr, "disruptor '%s/%s' error: ", d->address, d->username );
    fprintf( stderr, fmt, (char*)(&fmt + 1) );
}

