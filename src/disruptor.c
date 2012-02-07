#include "disruptor.h"

#include "util.h"
#include "zmalloc.h"
#include "shmem.h"
#include "shmap.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <hiredis/hiredis.h>

/* constants. */
#define MAX_ADDRESS_LENGTH      31
#define MAX_USERNAME_LENGTH     31
#define MAX_CONNECTIONS         256
#define MAX_SLOTS               4096

/* types */
typedef struct cursor
{
    volatile int64_t value;
    volatile int64_t padding[7];
} cursor;

typedef struct sharedConn
{
    volatile cursor readCursor;
} sharedConn;

typedef struct sharedHeader
{
    volatile int64_t session;
} sharedHeader;

typedef struct sharedSlot
{
    volatile int64_t timestamp;
    volatile int64_t handle;
    volatile int64_t padding[6];
} sharedSlot;

typedef struct sharedRingbuffer
{
    volatile cursor publishCursor;
    volatile cursor claimCursor;
    volatile sharedConn connections[ MAX_CONNECTIONS ];
    volatile sharedSlot slots[ MAX_SLOTS ];
} sharedRingbuffer;

typedef struct sendBuffer
{
    shmem* shmem;
    char* start;
    char* end;
} sendBuffer;

struct disruptor
{
    char* address;
    char* username;
    int64_t sendBufferSize;

    int id;
    int connectionsCount;

    redisContext* redis;

    shmem* shHeader;
    sharedHeader* header;

    shmem* shRingbuffer;
    sharedRingbuffer* ringbuffer;

    sendBuffer buffers[ MAX_CONNECTIONS ];
};

/* forward declarations. */
static bool startup( disruptor* d );
static void shutdown( disruptor* d );
static void handleError( disruptor* d, const char* fmt, ... );
static void handleInfo( disruptor* d, const char* fmt, ... );
static bool isStringValid( const char* str, size_t minSize, size_t maxSize );
static bool mapClient( disruptor* d, unsigned int id );
static void unmapClient( disruptor* d, unsigned int id );

/*-----------------------------------------------------------------------------
* Public API definitions.
*----------------------------------------------------------------------------*/

disruptor* disruptorCreate( const char* address, const char* username, int64_t sendBufferSize )
{
    disruptor* d = zcalloc( sizeof(disruptor) );
    d->address = strclone( address );
    d->username = strclone( username );
    d->sendBufferSize = sendBufferSize;
    if ( !startup( d ) )
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

static bool startup( disruptor* d )
{
    bool wasCreated = false;
    redisContext* r;
    redisReply* reply;

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

    /* connect to redis. */
    {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 500000;
        d->redis = redisConnectWithTimeout( "127.0.0.1", 6379, tv );
        if ( !d->redis )
        {
            handleError( d, "could not connect to redis" );
            return false;
        }

        r = d->redis;
    }

    /* determine a mapping for this connection. */
    {
        int id = -1;

        /* try to fetch an existing mapping. */
        {
            reply = redisCommand( r, "GET disruptor:%s:connections:%s:id", d->address, d->username );
            if ( reply->type == REDIS_REPLY_STRING )
            {
                id = atoi( reply->str );
            }
            freeReplyObject( reply );
        }

        /* if no mapping exists, then assign a new one. */
        if ( id < 0 )
        {
            {
                reply = redisCommand( r, "INCR disruptor:%s:connectionsCount", d->address );
                if ( reply->type == REDIS_REPLY_INTEGER )
                    id = ( reply->integer - 1 );
                freeReplyObject( reply );
            }

            assert( id >= 0 );
            if ( id < 0 )
            {
                handleError( d, "could not determine mapping for username '%s'", d->username );
                return false;
            }

            wasCreated = true;

            {
                reply = redisCommand( r, "SET disruptor:%s:connections:%s:id %d", d->address, d->username, id );
                freeReplyObject( reply );
            }

            {
                reply = redisCommand( r, "SET disruptor:%s:%d:username %s", d->address, id, d->username );
                freeReplyObject( reply );
            }
        }

        d->id = id;
    }

    /* determine the connection count. */
    {
        d->connectionsCount = 0;

        reply = redisCommand( r, "GET disruptor:%s:connectionsCount", d->address, d->username );
        if ( reply->type == REDIS_REPLY_STRING )
            d->connectionsCount = atoi( reply->str );
        freeReplyObject( reply );

        if ( d->connectionsCount <= 0 )
        {
            handleError( d, "could not determine the connection count." );
            return false;
        }
    }

    handleInfo( d, "id=%d total=%d", d->id, d->connectionsCount );

    /* open the shared header. */
    {
        d->shHeader = shmemOpen( sizeof(sharedHeader), SHMEM_DEFAULT, "disruptor:%s", d->address );
        d->header = shmemGetPtr( d->shHeader );
    }

    /* open the shared ringbuffer. */
    {
        d->shRingbuffer = shmemOpen( sizeof(sharedRingbuffer), SHMEM_DEFAULT, "disruptor:%s:rb", d->address );
        d->header = shmemGetPtr( d->shRingbuffer );
    }

    {
        int i;

        /* create the shared memory sendBuffer. */
        if ( wasCreated )
        {
            shmem* s;
            handleInfo( d, "creating %d", d->id );
            s = shmemOpen( d->sendBufferSize, SHMEM_MUST_CREATE, "disruptor:%s:%d", d->address, d->id );
            shmemClose( s );
        }

        /* map each connection. */
        for ( i = 0; i < d->connectionsCount; ++i )
        {
            if ( !mapClient( d, i ) )
            {
                handleError( d, "could not map client %d", i );
                return false;
            }
        }
    }

    return true;
}

static void shutdown( disruptor* d )
{
    if ( d->redis )
    {
        redisFree( d->redis );
        d->redis = NULL;
    }

    shmemClose( d->shHeader );
    d->shHeader = NULL;
    d->header = NULL;
}

static void handleError( disruptor* d, const char* fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    fprintf( stderr, "disruptor '%s/%s' error: ", d->address, d->username );
    vfprintf( stderr, fmt, ap );
    fprintf( stderr, "\n" );
    va_end( ap );
}

static void handleInfo( disruptor* d, const char* fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    fprintf( stdout, "disruptor '%s/%s' info: ", d->address, d->username );
    vfprintf( stdout, fmt, ap );
    fprintf( stdout, "\n" );
    va_end( ap );
}

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

static bool mapClient( disruptor* d, unsigned int id )
{
    assert( id < MAX_CONNECTIONS );
    if ( id >= MAX_CONNECTIONS )
        return false;

    unmapClient( d, id );

    {
        shmem* s;
        int64_t size;

        s = shmemOpen( 0, SHMEM_MUST_NOT_CREATE, "disruptor:%s:%d", d->address, id );
        if ( !s )
            return false;

        size = shmemGetSize( s );
        d->buffers[ id ].shmem = s;
        d->buffers[ id ].start = shmemGetPtr( s );
        d->buffers[ id ].end = ( d->buffers[ id ].start + size );
        handleInfo( d, "for #%d: size=%u", id, (unsigned int)size );
        return true;
    }
}

static void unmapClient( disruptor* d, unsigned int id )
{
    assert( id < MAX_CONNECTIONS );
    if ( id >= MAX_CONNECTIONS )
        return;

    {
        shmemClose( d->buffers[ id ].shmem );
        d->buffers[ id ].shmem = NULL;
        d->buffers[ id ].start = NULL;
        d->buffers[ id ].end = NULL;
    }
}

