CXX=c++
LIBS=$(shell pkg-config sdl2 --libs)
CXXFLAGS= -D__LINUX__ -Wall $(shell pkg-config sdl2 --cflags)

all: sdl2_play

sdl2_play: main.o
	${CXX} main.o $(LIBS) -o sdl2_play

main: main.cpp
	${CXX} ${CXXFLAGS} main.cpp

clean:
	rm -f main *.o
