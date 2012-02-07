#include "disruptor.h"

#include "util.h"
#include "zmalloc.h"
#include "shmem.h"
#include "shmap.h"
#include "atomics.h"

#include <hiredis/hiredis.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

/* constants. */
#define MAX_ADDRESS_LENGTH      31
#define MAX_USERNAME_LENGTH     31
#define MAX_CONNECTIONS         256
#define MAX_SLOTS               4096
#define SLOTS_MASK              4095

/* types */
typedef struct cursor
{
    volatile int64_t v;
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
    volatile int64_t sender;
    volatile int64_t size;
    volatile int64_t offset;
    volatile int64_t padding[4];
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
    char* tail;
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
    char* names[ MAX_CONNECTIONS ];

    int64_t readStart;
    int64_t readEnd;
};

/* forward declarations. */
static bool startup( disruptor* d );
static void shutdown( disruptor* d );
static void handleError( disruptor* d, const char* fmt, ... );
static void handleInfo( disruptor* d, const char* fmt, ... );
static bool isStringValid( const char* str, size_t minSize, size_t maxSize );
static redisContext* connectToRedis();
static bool mapClient( disruptor* d, unsigned int id );
static void unmapClient( disruptor* d, unsigned int id );
static bool waitUntilAvailable( disruptor* d, int64_t cursor );
static volatile sharedSlot* getSlot( disruptor* d, int64_t cursor );
static int64_t getMinimumCursor( disruptor* d );

/*-----------------------------------------------------------------------------
* Public API definitions.
*----------------------------------------------------------------------------*/

void disruptorKill( const char* address )
{
    redisContext* r;
    redisReply* reply;
    
    r = connectToRedis();
    if ( !r )
    {
        handleError( NULL, "failed to connect to redis." );
        return;
    }

    {
        reply = redisCommand( r, "KEYS disruptor:%s:*", address );
        if ( reply->type != REDIS_REPLY_ARRAY )
        {
            handleError( NULL, "failed to get keys." );
        }

        {
            size_t i;
            for ( i = 0; i < reply->elements; ++i )
            {
                redisReply* elem = reply->element[i];
                if ( elem && elem->type == REDIS_REPLY_STRING )
                {
                    const char* key = elem->str;
                    redisReply* reply2 = redisCommand( r, "DEL %s", key );
                    freeReplyObject( reply2 );
                }
            }
        }

        freeReplyObject( reply );
    }

    shmemUnlink( "disruptor:%s", address );
    shmemUnlink( "disruptor:%s:rb", address );

    {
        int i;
        for ( i = 0; i < MAX_CONNECTIONS; ++i )
        {
            shmemUnlink( "disruptor:%s:%d", address, i );
        }
    }
}

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

bool disruptorSend( disruptor* d, const char* msg, size_t size )
{
    char* result;
    
    result = disruptorClaim( d, size );
    if ( !result )
        return false;

    memcpy( result, msg, size );
    return disruptorPublish( d, result );
}

bool disruptorPrintf( disruptor* d, const char* format, ... )
{
    bool result;
    va_list ap;

    va_start( ap, format );
    result = disruptorVPrintf( d, format, ap );
    va_end( ap );
    return result;
}

bool disruptorVPrintf( disruptor* d, const char* format, va_list ap )
{
    size_t len;
    char* msg;

    /* determine the length of the resultant string. */
    {
        int ret;
        va_list tmp = ap;
        ret = vsnprintf( NULL, 0, format, tmp );
        assert( ret >= 0 );
        len = (size_t)( ret + 1 );
    }

    /* claim a message of that size. */
    {
        msg = disruptorClaim( d, (len + 1) );
        if ( !msg )
            return false;
    }
    msg[ len ] = '\0';

    /* format the message. */
    vsnprintf( msg, len, format, ap );

    /*handleInfo( d, "disruptorPrintf: %s", msg );*/
    return disruptorPublish( d, msg );
}

char* disruptorClaim( disruptor* d, size_t size )
{
    char* result;
    sendBuffer* buf;
    
    buf = &d->buffers[ d->id ];

    /* full? */
    if ( buf->tail + size > buf->end )
        return NULL;
    
    result = buf->tail;
    buf->tail += size;
    return result;
}

bool disruptorPublish( disruptor* d, char* ptr )
{
    int64_t claim;
    sendBuffer* buf;
    volatile sharedSlot* slot;

    (void)d;
    (void)ptr;
    buf = &d->buffers[ d->id ];

    /* increment the claim cursor. */
    claim = xadd64( &d->ringbuffer->claimCursor.v, 1 );

    /* block until the slot is ready. */
    if ( !waitUntilAvailable( d, claim ) )
        return false;

    /* fill out the slot. */
    {
        slot = getSlot( d, claim );
        assert( slot );
        if ( !slot )
            return false;

        slot->sender = d->id;
        slot->size = (buf->tail - ptr);
        slot->offset = (ptr - buf->start);
        slot->timestamp = rdtsc();

        /*
        handleInfo( d, "slot %lld sender=%lld size=%lld offset=%lld timestamp=%lld",
                claim,
                slot->sender,
                slot->size,
                slot->offset,
                slot->timestamp );
                */
    }


    /* wait until any other producers have published. */
    {
        int64_t expectedCursor = ( claim - 1 );
        while ( d->ringbuffer->publishCursor.v < expectedCursor )
        {
            atomicYield();
        }
    }

    /* increment the publish cursor. */
    d->ringbuffer->publishCursor.v = claim;

    handleInfo( d, "publish %d", (int)claim );

    return true;
}

disruptorMsg disruptorRecv( disruptor* d )
{
    volatile sharedConn* conn = &d->ringbuffer->connections[ d->id ];

    if ( d->readEnd > 0 )
    {
        if ( d->readStart < d->readEnd )
        {
            d->readStart += 1;
            return d->readStart;
        }

        if ( d->readEnd > conn->readCursor.v )
            conn->readCursor.v = d->readEnd;

        d->readStart = d->readEnd = 0;
    }

    {
        int64_t publishCursor = d->ringbuffer->publishCursor.v;

        if ( conn->readCursor.v >= publishCursor )
            return 0;

        d->readStart = conn->readCursor.v + 1;
        d->readEnd = publishCursor + 1;
        return d->readStart;
    }
}

char* msgGetData( disruptor* d, disruptorMsg m )
{
    volatile sharedSlot* slot;
    sendBuffer* buf;
    
    slot = getSlot( d, m - 1 );
    assert( slot );
    buf = &d->buffers[ slot->sender ];

    return &buf->start[ slot->offset ];
}

size_t msgGetSize( disruptor* d, disruptorMsg m )
{
    volatile sharedSlot* slot;
    slot = getSlot( d, m - 1 );
    assert( slot );

    return slot->size;
}

int64_t msgGetSequence( disruptor* d, disruptorMsg m )
{
    (void)d;

    return (m - 1);
}

int64_t msgGetTimestamp( disruptor* d, disruptorMsg m )
{
    volatile sharedSlot* slot;
    slot = getSlot( d, m - 1 );
    assert( slot );

    return slot->timestamp;
}

const char* msgGetSender( disruptor* d, disruptorMsg m )
{
    int id;

    id = msgGetSenderId( d, m );
    assert( id >= 0 && id < MAX_CONNECTIONS );
    assert( d->names[ id ] != NULL );

    return d->names[ id ];
}

int msgGetSenderId( disruptor* d, disruptorMsg m )
{
    volatile sharedSlot* slot;
    slot = getSlot( d, m - 1 );
    assert( slot );

    return (int)slot->sender;
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
    r = d->redis = connectToRedis();
    if ( !r )
    {
        handleError( d, "could not connect to redis" );
        return false;
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
        d->ringbuffer = shmemGetPtr( d->shRingbuffer );
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
    int i;

    for ( i = 0; i < MAX_CONNECTIONS; ++i )
        unmapClient( d, i );

    if ( d->redis )
    {
        redisFree( d->redis );
        d->redis = NULL;
    }

    shmemClose( d->shRingbuffer );
    d->shRingbuffer = NULL;
    d->ringbuffer = NULL;

    shmemClose( d->shHeader );
    d->shHeader = NULL;
    d->header = NULL;
}

static void handleError( disruptor* d, const char* fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    if ( d )
        fprintf( stderr, "disruptor '%s/%s' error: ", d->address, d->username );
    else
        fprintf( stderr, "disruptor error: " );
    vfprintf( stderr, fmt, ap );
    fprintf( stderr, "\n" );
    va_end( ap );
}

static void handleInfo( disruptor* d, const char* fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    if ( d )
        fprintf( stdout, "disruptor '%s/%s' info: ", d->address, d->username );
    else
        fprintf( stdout, "disruptor info: " );
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

static redisContext* connectToRedis()
{
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 500000;
    return redisConnectWithTimeout( "127.0.0.1", 6379, tv );
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
        d->buffers[ id ].tail = d->buffers[ id ].start;
        handleInfo( d, "for #%d: size=%u", id, (unsigned int)size );

        {
            redisContext* r = d->redis;
            redisReply* reply;

            reply = redisCommand( r, "GET disruptor:%s:%d:username", d->address, id );
            if ( reply->type == REDIS_REPLY_STRING )
                d->names[ id ] = strclone( reply->str );
            freeReplyObject( reply );
        }

        if ( !d->names[ id ] )
            return false;

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
        d->buffers[ id ].tail = NULL;
        d->buffers[ id ].end = NULL;
    }

    strfree( d->names[ id ] );
    d->names[ id ] = NULL;
}

static bool waitUntilAvailable( disruptor* d, int64_t cursor )
{
    int64_t wrapPoint = ( ( cursor + 1 ) - MAX_SLOTS );
    while ( wrapPoint > getMinimumCursor( d ) )
    {
        atomicYield();
    }
    return true;
}

static volatile sharedSlot* getSlot( disruptor* d, int64_t cursor )
{
    size_t at = (size_t)(cursor & SLOTS_MASK);
    return &d->ringbuffer->slots[ at ];
}

static int64_t getMinimumCursor( disruptor* d )
{
    (void)d;
    return ~(int64_t)0;
}

