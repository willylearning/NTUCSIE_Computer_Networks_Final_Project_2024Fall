how to compile:
make

how to run:
./server
./client

how to clean:
make clean

openssl:
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr
openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

chatting command: (using relay mode)
If user kevin wants to send "hello" to user james => send kevin james hello
user james wants to see who sends messages to him => check msg
user james should receive "Message from kevin: hello"

sending file command: (using relay mode)
If user kevin wants to send "apple.jpg" to user james => file kevin james apple.jpg
user james wants to see who sends messages to him => check msg
user james should receive "File from kevin: apple.jpg", and james should see a received_apple.jpg in his folder

streaming command: (using relay mode)
If user james wants to listen to someone streaming to him => check streaming
Meanwhile, if user kevin wants to stream "s10.wav" to user james => streaming kevin james s10.wav
Then after user kevin types the command, user james will hear streaming of s10.wav