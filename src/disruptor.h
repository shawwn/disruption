#ifndef __DISRUPTOR_H__
#define __DISRUPTOR_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* bool. */
#ifndef bool
# define bool   int
# define true   1
# define false  0
#endif

/*-----------------------------------------------------------------------------
* Data types
*----------------------------------------------------------------------------*/

struct disruptor;
typedef struct disruptor disruptor;

typedef int64_t disruptorMsg;

/*-----------------------------------------------------------------------------
* Function prototypes
*----------------------------------------------------------------------------*/

void disruptorKill( const char* address );
disruptor* disruptorCreate( const char* address, const char* username, int64_t sendBufferSize );
void disruptorRelease( disruptor* d );

bool disruptorSend( disruptor* d, const char* msg, size_t size );
bool disruptorPrintf( disruptor* d, const char* format, ... );
bool disruptorVPrintf( disruptor* d, const char* format, va_list ap );

char* disruptorClaim( disruptor* d, size_t size );
bool disruptorPublish( disruptor* d, char* ptr );

disruptorMsg disruptorRecv( disruptor* d );
char* msgGetData( disruptor* d, disruptorMsg m );
size_t msgGetSize( disruptor* d, disruptorMsg m );
int64_t msgGetSequence( disruptor* d, disruptorMsg m );
int64_t msgGetTimestamp( disruptor* d, disruptorMsg m );
const char* msgGetSender( disruptor* d, disruptorMsg m );
int msgGetSenderId( disruptor* d, disruptorMsg m );

#endif

