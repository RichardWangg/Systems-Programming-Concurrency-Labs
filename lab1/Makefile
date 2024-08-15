CFLAGS = -Wall -g 
LDFLAGS = -g

all: findpng catpng

findpng: findpng.o ./starter/png_util/crc.o 
	gcc -g -o $@ $^ -lz

catpng: catpng.o ./starter/png_util/crc.o ./starter/png_util/zutil.o
	gcc -g -o $@ $^ -lz

# /starter/png_util/crc: crc.o
# 	gcc -g -o $@ $^ -lz

.c.o:
	gcc -Wall -g -c $<

%.d: %.c
	gcc -MM -MF $@ $<

.PHONY: clean
clean:
	rm -f *.d *.o findpng catpng
