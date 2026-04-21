CC = gcc
CXX = g++
CFLAGS = -Wall -g -pthread
 
all: hw2
 
hw2: main.o hw2_output.o crossroad.o
	$(CXX) $(CFLAGS) -o hw2 main.o hw2_output.o crossroad.o
 
main.o: main.cpp crossroad.h hw2_output.h
	$(CXX) $(CFLAGS) -c main.cpp
 
crossroad.o: crossroad.cpp crossroad.h hw2_output.h
	$(CXX) $(CFLAGS) -c crossroad.cpp
 
hw2_output.o: hw2_output.c hw2_output.h
	$(CC) $(CFLAGS) -c hw2_output.c
 
clean:
	rm -f *.o hw2
 