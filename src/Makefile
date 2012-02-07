# Disruption Makefile
# Copyright (C) 2012 Shawn Presser <shawnpresser at gmail dot com>
# Copyright (C) 2009 Salvatore Sanfilippo <antirez at gmail dot com>
# This file is released under the BSD license, see the COPYING file

OPTIMIZATION?=-O2


CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

ifndef V
QUIET_CC = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR);
QUIET_LINK = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR);
endif

ALLOC_DEP=
ALLOC_LINK=
ALLOC_FLAGS=

CFLAGS?=-std=c99 -pedantic $(OPTIMIZATION) -Wall -W
CCLINK?=-lrt -lhiredis
DEBUG?=-g -rdynamic -ggdb

CCOPT= $(CFLAGS) $(ARCH) $(PROF)

PREFIX= /usr/local
INSTALL_BIN= $(PREFIX)/bin
INSTALL= cp -p

OBJ = disruptor.o util.o zmalloc.o shmem.o shmap.o
BENCHOBJ = $(OBJ) disruptor-benchmark.o

BENCHPRGNAME = disruptor-benchmark

all: disruptor-benchmark

# Deps (use make dep -o generate this)
disruptor-benchmark.o: disruptor-benchmark.c disruptor.h util.h
disruptor.o: disruptor.c disruptor.h util.h zmalloc.h shmem.h shmap.h
shmap.o: shmap.c shmap.h util.h zmalloc.h
shmem.o: shmem.c shmem.h util.h zmalloc.h
util.o: util.c util.h zmalloc.h
zmalloc.o: zmalloc.c zmalloc.h

.PHONY: dependencies

dependencies:

uisruptor-benchmark.o:

	$(QUIET_CC)$(CC) -c $(CFLAGS) $(DEBUG) $(COMPILE_TIME) $<

disruptor-benchmark: dependencies $(BENCHOBJ)
	$(QUIET_LINK)$(CC) -o $(BENCHPRGNAME) $(CCOPT) $(DEBUG) $(BENCHOBJ) $(CCLINK) $(ALLOC_LINK)

%.o: %.c $(ALLOC_DEP)
	$(QUIET_CC)$(CC) -c $(CFLAGS) $(ALLOC_FLAGS) $(DEBUG) $(COMPILE_TIME) $<

clean:
	rm -rf $(BENCHPRGNAME) *.o *.gcda *.gcno *.gcov

dep:
	$(CC) -MM *.c

test:

bench:
	./disruptor-benchmark

32bit:
	$(MAKE) ARCH="-m32"

gprof:
	$(MAKE) PROF="-pg"

gcov:
	$(MAKE) PROF="-fprofile-arcs -ftest-coverage"

noopt:
	$(MAKE) OPTIMIZATION=""

32bitgprof:
	$(MAKE) PROF="-pg" ARCH="-arch i386"

install: all
	mkdir -p $(INSTALL_BIN)
	$(INSTALL) $(BENCHPRGNAME) $(INSTALL_BIN)
