# Toplevel makefile; the real one is in src/Makefile

TARGETS= 32bit noopt test

all:
	cd src && $(MAKE) $@

install: dummy
	cd src && $(MAKE) $@

clean:
	cd src && $(MAKE) $@

$(TARGETS):
	cd src && $(MAKE) $@

dummy:
