CC = gcc
CFLAGS_XML2 = $(shell xml2-config --cflags)
CFLAGS_CURL = $(shell curl-config --cflags)
CFLAGS = -Wall $(CFLAGS_XML2) $(CFLAGS_CURL) -std=gnu99 -g -DDEBUG1_
LD = gcc
LDFLAGS = -std=gnu99 -g
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS = $(LDLIBS_XML2) $(LDLIBS_CURL)
LIBS = -lcurl -lpthread -lz

all: findpng2

findpng2: findpng2.o helpers.o
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) $(LIBS)

findpng2.o: findpng2.c
	$(CC) $(CFLAGS) -c $< -o $@

helpers.o: helpers.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.d *.o findpng2