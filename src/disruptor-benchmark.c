#include <stdio.h>

#include "disruptor.h"
#include "util.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    if ( 0 )
    {
        char* test = strformat( "Hello, %s", argv[0] );
        printf( "%s\n", test );
        strfree( test );
    }

    if ( 1 )
    {
        disruptor* d = disruptorCreate( "benchmark", "client", 16*1024 );

        disruptorRelease( d );
    }

    return 0;
}

