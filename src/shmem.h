#ifndef __DISRUPTOR_SHMEM_H__
#define __DISRUPTOR_SHMEM_H__

#include <stdint.h> 
/*-----------------------------------------------------------------------------
* Declarations
*----------------------------------------------------------------------------*/

struct shmem;
typedef struct shmem shmem;

/* flags. */
#define SHMEM_MUST_CREATE       (1 << 0)
#define SHMEM_MUST_NOT_CREATE   (1 << 1)
#define SHMEM_DEFAULT           0

/*-----------------------------------------------------------------------------
* Function prototypes
*----------------------------------------------------------------------------*/

void shmemUnlink( const char* formatName, ... );
shmem* shmemOpen( int64_t size, int flags, const char* formatName, ... );
void shmemClose( shmem* s );
int64_t shmemGetSize( shmem* s );
void* shmemGetPtr( shmem* s );

#endif

