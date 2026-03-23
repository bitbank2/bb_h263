CFLAGS=-c -Wall -O2 -D__LINUX__
LIBS = -lm -lpthread

all: hello_cpp

hello_cpp: main.o bb_h263.h
	$(CC) main.o $(LIBS) -o hello_cpp

main.o: main.cpp
	$(CXX) $(CFLAGS) main.cpp

clean:
	rm *.o hello_cpp
