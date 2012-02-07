# Disruption Makefile
# Copyright (C) 2012 Shawn Presser <shawnpresser at gmail dot com>
# Copyright (C) 2009 Salvatore Sanfilippo <antirez at gmail dot com>
# This file is released under the BSD license, see the COPYING file

OPTIMIZATION?=-O2

ALLOC_DEP=
ALLOC_LINK=
ALLOC_FLAGS=

CFLAGS?=
CCLINK?=
DEBUG?=

CCOPT= $(CFLAGS) $(ARCH) $(PROF)

PREFIX= /usr/local
INSTALL_BIN= $(PREFIX)/bin
INSTALL= cp -p

OBJ = disruptor.o
BENCHOBJ = $(OBJ) disruptor-benchmark.o

BENCHPRGNAME = disruptor-benchmark

all: disruptor-benchmark


# Deps (use make dep -o generate this)
disruptor-benchmark.o: disruptor-benchmark.c disruptor.h
disruptor.o: disruptor.c disruptor.h

.PHONY: dependencies

dependencies:

disruptor-benchmark.o:
	$(CC) -c $(CFLAGS) $(DEBUG) $(COMPILE_TIME) $<

disruptor-benchmark: dependencies $(BENCHOBJ)
	$(CC) -o $(BENCHPRGNAME) $(CCOPT) $(DEBUG) $(BENCHOBJ) $(CCLINK) $(ALLOC_LINK)

%.o: %.c $(ALLOC_DEP)
	$(CC) -c $(CFLAGS) $(ALLOC_FLAGS) $(DEBUG) $(COMPILE_TIME) $<

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
