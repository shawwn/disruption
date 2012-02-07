#include "disruptor.h"
#include "util.h"

#include <stdio.h>

#include <sys/wait.h>
#include <unistd.h>


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
        disruptorKill( "benchmark" );
    }

    {
        int fd;
        bool child;

        if ( 1 )
            fd = fork();
        else
            fd = 0;
        child = (fd == 0);

        if ( 1 )
        {
            disruptorMsg m;
            disruptor* d = disruptorCreate( "benchmark", (child ? "clientB" : "clientA"), 16*1024 );

            disruptorPrintf( d, "hello, world!" );
            disruptorPrintf( d, "hello again, world!" );

            while ( (m = disruptorRecv( d )) )
            {
                int64_t time = msgGetTimestamp( d, m );
                int senderId = msgGetSenderId( d, m );
                const char* sender = msgGetSender( d, m );
                size_t size = msgGetSize( d, m );
                char* msg = msgGetData( d, m );
                printf( "received time=%lld sender=%s size=%d msg=%s\n",
                        time, sender, (int)size, msg );
            }

            disruptorRelease( d );
        }

        if ( !child )
        {
            int signal;
            wait( &signal );
        }
    }

    return 0;
}

