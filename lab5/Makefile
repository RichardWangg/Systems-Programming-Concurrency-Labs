CC = gcc
CFLAGS_XML2 = $(shell xml2-config --cflags)
CFLAGS_CURL = $(shell curl-config --cflags)
CFLAGS = -Wall $(CFLAGS_XML2) $(CFLAGS_CURL) -std=gnu99 -g
LD = gcc
LDFLAGS = -std=gnu99 -g
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS = $(LDLIBS_XML2) $(LDLIBS_CURL)
LIBS = -lcurl -lpthread -lz

all: findpng3

findpng2: findpng3.o helpers.o
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) $(LIBS)


.PHONY: clean
clean:
	rm -f *.d *.o findpng3 helpers
