CPP = g++
CPPFLAGS = -Wall -std=c++11

all: oss worker

oss: oss.cpp
	$(CPP) $(CPPFLAGS) -o oss oss.cpp

worker: worker.cpp
	$(CPP) $(CPPFLAGS) -o worker worker.cpp

clean:
	rm -f oss worker
