#define _LARGEFILE64_SOURCE
#define _USE_FILE_OFFSET64
#define _USE_LARGEFILE64
#include "shmem.h"

#include "util.h"
#include "zmalloc.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

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
static void handleInfo( shmem* s, const char* fmt, ... );
static void platformUnlink( const char* name );
static bool platformStartup( shmem* s );
static void platformShutdown( shmem* s );

/*-----------------------------------------------------------------------------
* Public API definitions.
*----------------------------------------------------------------------------*/

void shmemUnlink( const char* formatName, ... )
{
    va_list ap;
    va_start( ap, formatName );
    {
        char* fullname = strformat( formatName, ap );
        handleInfo( NULL, "shmemUnlink('%s')", fullname );
        platformUnlink( fullname );
        strfree( fullname );
    }
    va_end( ap );
}

shmem* shmemOpen( int64_t size, int flags, const char* formatName, ... )
{
    shmem* s = zcalloc( sizeof( shmem ) );
    {
        va_list ap;
        va_start( ap, formatName );
        s->name = vstrformat( formatName, ap );
        va_end( ap );
    }
    s->size = size;
    s->flags = flags;
    handleInfo( s, "open(size=%u, flags=%d)", (unsigned int)size, flags );
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

    handleInfo( s, "close()" );
    shutdown( s );
    strfree( s->name );
    zfree( s );
}

int64_t shmemGetSize( shmem* s )
{
    if ( !s )
        return 0;

    return s->size;
}

void* shmemGetPtr( shmem* s )
{
    if ( !s )
        return NULL;

    return s->mapped;
}

/*-----------------------------------------------------------------------------
* File-local function definitions.
*----------------------------------------------------------------------------*/

static bool startup( shmem* s )
{
    /* validate inputs. */
    {
        /* too short? */
        if ( strlen( s->name ) <= 0 )
        {
            handleError( s, "name too short." );
            return false;
        }

        /* check for slashes in the name. */
        if ( strpbrk( s->name, "/\\" ) )
        {
            handleError( s, "name may not contain slashes." );
            return false;
        }
    }

    /* validate flags. */
    {
        bool mustCreate = (s->flags & SHMEM_MUST_CREATE);
        bool mustNotCreate = (s->flags & SHMEM_MUST_NOT_CREATE);

        if ( mustCreate || mustNotCreate )
        {
            if ( mustCreate == mustNotCreate )
            {
                handleError( s, "must specify either SHMEM_MUST_CREATE or SHMEM_MUST_NOT_CREATE, not both" );
                return false;
            }
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
    va_list ap;
    va_start( ap, fmt );
    if ( s )
        fprintf( stderr, "shmem('%s') error: ", s->name );
    else
        fprintf( stderr, "shmem error: " );
    vfprintf( stderr, fmt, ap );
    fprintf( stderr, "\n" );
    va_end( ap );
}

static void handleInfo( shmem* s, const char* fmt, ... )
{
    va_list ap;
    va_start( ap, fmt );
    if ( s )
        fprintf( stdout, "shmem('%s') info: ", s->name );
    else
        fprintf( stdout, "shmem info: " );
    vfprintf( stdout, fmt, ap );
    fprintf( stdout, "\n" );
    va_end( ap );
}

/*-----------------------------------------------------------------------------
* Platform-specific function definitions.
*----------------------------------------------------------------------------*/

#if _MSC_VER
# include <windows.h>
#else
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <fcntl.h>
extern int ftruncate64( int fd, off64_t length );
#endif

#if _MSC_VER
#error "TODO"
#else
static void platformUnlink( const char* name )
{
    char* fullname = strformat( "/%s", name );
    shm_unlink( fullname );
    strfree( fullname );
}

static bool platformStartup( shmem* s )
{
    bool mustCreate = (s->flags & SHMEM_MUST_CREATE);
    bool mustNotCreate = (s->flags & SHMEM_MUST_NOT_CREATE);
    int shmFlags;
    int shmMode;
    int protFlags;

    /* build the flags. */
    {
        shmFlags = 0;
        if ( mustCreate )
        {
            shmFlags = ( O_RDWR | O_CREAT | O_EXCL );
        }
        else if ( mustNotCreate )
        {
            shmFlags = ( O_RDWR );
        }
        else
        {
            shmFlags = ( O_RDWR | O_CREAT );
        }
    }
    
    /* build the mode. */
    {
        shmMode = ( S_IRUSR | S_IWUSR );
    }

    /* build the protection flags. */
    {
        protFlags = ( PROT_READ | PROT_WRITE );
    }

    /* open the shared memory segment. */
    {
        char* fullname = strformat( "/%s", s->name );
        s->fd = shm_open( fullname, shmFlags, shmMode );
        strfree( fullname );

        if ( s->fd < 0 )
        {
            handleError( s, "shm_open() error: %s", strerror(errno) );
            return false;
        }
    }

    /* get info for the shared memory. */
    {
        struct stat64 info;
        int ret;

        ret = fstat64( s->fd, &info );
        if ( ret < 0 )
        {
            handleError( s, "fstat64() error: %s", strerror(errno) );
            return false;
        }

        if ( info.st_blksize > s->size )
        {
            s->size = info.st_blksize;
        }
    }

    /* resize the shared memory. */
    {
        int ret;

        ret = ftruncate64( s->fd, s->size );
        if ( ret < 0 )
        {
            handleError( s, "ftruncate64() error: %s", strerror(errno) );
            return false;
        }
    }

    /* map the shared memory. */
    {
        s->mapped = mmap( NULL, s->size, protFlags, MAP_SHARED, s->fd, 0 );
        if ( s->mapped == MAP_FAILED )
        {
            handleError( s, "mmap() error: %s", strerror(errno) );
            return false;
        }
    }

    return true;
}

static void platformShutdown( shmem* s )
{
    /* unmap the memory. */
    if ( s->mapped )
    {
        munmap( s->mapped, s->size );
        s->mapped = NULL;
    }

    /* close the shared memory segment. */
    if ( s->fd >= 0 )
    {
        close( s->fd );
        s->fd = -1;
    }
}
#endif /* !_MSC_VER */

