CC = gcc
CXX = g++
CFLAGS = -std=c99 -O2 -Wall -Wextra -D_FILE_OFFSET_BITS=64
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -D_FILE_OFFSET_BITS=64

unrage.exe: unrage.o rpf.o
	$(CXX) -o $@ unrage.o rpf.o

unrage.o: unrage.cpp rpf.h
	$(CXX) $(CXXFLAGS) -c unrage.cpp

rpf.o: rpf.c rpf.h
	$(CC) $(CFLAGS) -c rpf.c

test: unrage.exe
	powershell -NoProfile -ExecutionPolicy Bypass -File test.ps1

clean:
	rm -f unrage unrage.exe unrage.o rpf.o
