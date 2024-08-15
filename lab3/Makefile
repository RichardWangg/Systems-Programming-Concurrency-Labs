CFLAGS = -Wall -g
LDFLAGS = -g

# Libraries needed for linking
LIBS = -lcurl -lpthread

all: paster2

paster2: paster2.o concat.o ./starter/png_util/crc.o ./starter/png_util/zutil.o
	gcc $(LDFLAGS) -o $@ $^ $(LIBS) -lz 

%.o: %.c
	gcc $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<

.PHONY: clean
clean:
	rm -f *.d *.o paster2 *~ *.png
