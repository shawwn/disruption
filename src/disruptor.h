#ifndef __DISRUPTOR_H__
#define __DISRUPTOR_H__

/*-----------------------------------------------------------------------------
* Data types
*----------------------------------------------------------------------------*/

struct disruptor;
typedef struct disruptor disruptor;

/*-----------------------------------------------------------------------------
* Function prototypes
*----------------------------------------------------------------------------*/

disruptor* disruptorCreate( const char* address, const char* userName );
void disruptorRelease( disruptor* d );

#endif

