CFLAGS=-c -Wall -O2 -I../src
LIBS = -lm -lpthread

all: libbb_h263.a

libbb_h263.a: bb_h263.o
	ar -rc libbb_h263.a bb_h263.o ;\
	sudo cp libbb_h263.a /usr/local/lib ;\
	sudo cp ../src/bb_h263.h /usr/local/include

bb_h263.o: ../src/bb_h263.cpp ../src/bb_h263.h
	$(CXX) $(CFLAGS) -D__LINUX__ bb_h263.cpp

clean:
	rm *.o libbb_h263.a
