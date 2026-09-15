CFLAGS=-Wall -O2 -g -fsanitize=address,undefined -DREQUIRES_ALIGNED
LIBS = 

all: h263_test

h263_test: main.o
	$(CC) main.o -fsanitize=address,undefined -g $(LIBS) -o h263_test 

main.o: main.cpp ../../../src/bb_h263.h
	$(CXX) $(CFLAGS) -c main.cpp

clean:
	rm -rf *.o h263_test
