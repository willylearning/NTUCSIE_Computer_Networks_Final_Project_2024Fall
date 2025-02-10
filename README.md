# NTUCSIE Computer Networks Socket Programming Project 2024 Fall
## Overview
The project is a real-time online chatroom application that allows users to communicate through various media types, including text, files, and live video streaming. The system is designed to support seamless and secure communication in both private and public chat modes. Check CSIE3510 Socket Programming Project.pdf for knowing more details.

## How to compile:
make

## How to run:
./server
./client

## How to clean:
make clean

## Set openssl:
openssl genrsa -out server.key 2048  
openssl req -new -key server.key -out server.csr  
openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

## Chatting command: (using relay mode)
If user kevin wants to send "hello" to user james => send kevin james hello  
user james wants to see who sends messages to him => check msg  
user james should receive "Message from kevin: hello"

## Sending file command: (using relay mode)
If user kevin wants to send "apple.jpg" to user james => file kevin james apple.jpg  
user james wants to see who sends messages to him => check msg  
user james should receive "File from kevin: apple.jpg", and james should see a received_apple.jpg in his folder

## Streaming command: (using relay mode)
If user james wants to listen to someone streaming to him => check streaming
Meanwhile, if user kevin wants to stream "s10.wav" to user james => streaming kevin james s10.wav
Then after user kevin types the command, user james will hear streaming of s10.wav
