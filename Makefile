# Makefile for chatroom application

CXX = g++
CXXFLAGS = -std=c++11 -O3 -g -Wno-unused-result
TARGETS = server client

all: $(TARGETS)

server: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server

client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client

clean:
	rm -f $(TARGETS)
