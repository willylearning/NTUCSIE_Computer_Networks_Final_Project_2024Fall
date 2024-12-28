# Makefile for chatroom application

CXX = g++
CXXFLAGS = -std=c++14 -O3 -g -Wno-unused-result
TARGETS = server client

all: $(TARGETS)

server: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server -lpthread -lssl -lcrypto 

client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client -lpthread -lssl -lcrypto -lopenal

clean:
	rm -f $(TARGETS)
