CXX = g++
CXXFLAGS = -Wall -O2

all: client server

client: client2.cpp utils2.cpp
	$(CXX) $(CXXFLAGS) -o client client2.cpp utils2.cpp

server: server.cpp utils.cpp
	$(CXX) $(CXXFLAGS) -o server server2.cpp utils2.cpp

clean:
	rm -f client server
