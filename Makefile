CXX = g++
CXXFLAGS = -Wall -O2

all: client server

client: client.cpp utils.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp utils.cpp

server: server.cpp utils.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp utils.cpp

clean:
	rm -f client server
