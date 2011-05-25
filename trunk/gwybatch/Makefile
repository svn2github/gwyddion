PKGCONFIG = pkg-config
DEPS_CFLAGS := $(shell $(PKGCONFIG) --cflags gwyddion)
DEPS_LDFLAGS := $(shell $(PKGCONFIG) --libs gwyddion)
MY_CFLAGS = -Wall -W -g -O2
MY_LDFLAGS =

all: gwybatch

gwybatch: gwybatch.c
	$(CC) $(DEPS_CFLAGS) $(MY_CFLAGS) $(CFLAGS) $(DEPS_LDFLAGS) $(MY_LDFLAGS) $(LDFLAGS) -o gwybatch gwybatch.c

clean:
	rm -f gwybatch

dist:
	rm -rf gwybatch
	mkdir gwybatch
	cp gwybatch.c Makefile COPYING gwybatch/
	tar -jcvf gwybatch.tar.bz2 gwybatch/
	rm -rf gwybatch

.PHONY: all clean dist
