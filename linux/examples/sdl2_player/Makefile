CXX=c++
LIBS=$(shell pkg-config sdl2 --libs)
CXXFLAGS= -ggdb -D__LINUX__ -Wall $(shell pkg-config sdl2 --cflags)

all: sdl2_play

sdl2_play: main.o ../../../src/bb_h263.h
	${CXX} -g main.o $(LIBS) -o sdl2_play

main: main.cpp
	${CXX} ${CXXFLAGS} main.cpp

clean:
	rm -f main *.o
