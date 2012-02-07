#ifndef __DISRUPTOR_H__
#define __DISRUPTOR_H__

#include <stdint.h>

/*-----------------------------------------------------------------------------
* Data types
*----------------------------------------------------------------------------*/

struct disruptor;
typedef struct disruptor disruptor;

/*-----------------------------------------------------------------------------
* Function prototypes
*----------------------------------------------------------------------------*/

disruptor* disruptorCreate( const char* address, const char* username, int64_t sendBufferSize );
void disruptorRelease( disruptor* d );

#endif

