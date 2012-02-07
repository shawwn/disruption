#ifndef __DISRUPTOR_SHMAP_H__
#define __DISRUPTOR_SHMAP_H__

#include <stdint.h> 
#include <stddef.h>
#include "util.h"

/*-----------------------------------------------------------------------------
* Declarations
*----------------------------------------------------------------------------*/

struct shmap;
typedef struct shmap shmap;

typedef int64_t SharedHandle;

/*-----------------------------------------------------------------------------
* Function prototypes
*----------------------------------------------------------------------------*/

shmap* shmapCreate( void* mem, int64_t memSize, size_t itemSize, const char* debugName, ... );
void shmapRelease( shmap* s );

const char* shmapGetKey( shmap* s, SharedHandle h );
void* shmapGetValue( shmap* s, SharedHandle h );
bool shmapIsNew( shmap* s, SharedHandle h );
void shmapSetNew( shmap* s, SharedHandle h );

SharedHandle shmapFindItem( shmap* s, const char* key );
SharedHandle shmapGetItem( shmap* s, const char* key );

SharedHandle shmapGetFirst( shmap* s );
SharedHandle shmapGetNext( shmap* s );

#endif

