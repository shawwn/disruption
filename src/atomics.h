#ifndef __DISRUPTOR_ATOMICS_H__
#define __DISRUPTOR_ATOMICS_H__

#include <stdint.h>
#include <stddef.h>

/*-----------------------------------------------------------------------------
* Declarations
*----------------------------------------------------------------------------*/

#define ATOMIC_INLINE static inline

/*-----------------------------------------------------------------------------
* Function prototypes (POSIX implementation)
*----------------------------------------------------------------------------*/
#ifndef _MSC_VER

#include <sched.h>
#include <sys/wait.h>

ATOMIC_INLINE int64_t rdtsc()
{
    int64_t ret;
    __asm__ volatile( "rdtsc"
            : "=A"( ret )
            :
            : );
    return ret;
}

ATOMIC_INLINE int64_t rdtscp()
{
    int64_t ret;
    __asm__ volatile( "rdtscp"
            : "=A"( ret )
            :
            : "ecx" );
    return ret;
}

ATOMIC_INLINE int64_t xadd64( volatile int64_t* v, int64_t delta )
{
    return __sync_add_and_fetch( v, delta );
}

ATOMIC_INLINE void atomicYield()
{
    sched_yield();
}

#endif // !_MSC_VER

#endif

