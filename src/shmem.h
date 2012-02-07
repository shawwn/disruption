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

shmem* shmemOpen( const char* name, int64_t size, int flags );
void shmemClose( shmem* s );

#endif

