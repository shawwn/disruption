#include <stdio.h>

#include "disruptor.h"
#include "util.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    {
        char* test = strformat( "Hello, %s", argv[0] );
        printf( "%s\n", test );
        strfree( test );
    }

    return 0;
}

