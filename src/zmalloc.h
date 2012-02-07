#ifndef __DISRUPTOR_ZMALLOC_H__
#define __DISRUPTOR_ZMALLOC_H__

#include <stddef.h>

void* zmalloc( size_t size );
void* zcalloc( size_t size );
void zfree( void* ptr );

#endif

