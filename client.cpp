#include <iostream>
#include <cstring>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <fcntl.h>
#include <AL/al.h>
#include <AL/alc.h>

using namespace std;

#define FRAME_SIZE 65536
#define BUFFER_COUNT 4
#define SAMPLING_RATE 22050

string login_username;

void initializeOpenSSL() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

SSL_CTX* createClientContext() {
    const SSL_METHOD* method = SSLv23_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void handleSignal(int signum) {
    cout << "\nLogout not allowed via direct interruption. Please use the logout command.\n";
}

int LogInProcess(SSL* ssl)
{
    signal(SIGINT, handleSignal);
    
    while(true)
    {
        cout << "Choose an option, 1.Register  2.Login  3.Quit : \n";
        // choose an option
        string userInput;
        cin >> userInput;
        // SSL_write(clientSocket, userInput.c_str(), userInput.size());
        SSL_write(ssl, userInput.c_str(), userInput.size());

        if (userInput == "1") { // register 
            // Create username
            cout << "Create username:\n";
            string username;
            cin >> username;
            SSL_write(ssl, username.c_str(), username.size());

            // Create password
            cout << "Create password:\n";
            string password;
            cin >> password;
            SSL_write(ssl, password.c_str(), password.size());

            // Registered successfully
            cout << "You have registered successfully!\n";
            
        }else if(userInput == "2"){ // logging in
            while(true){
                // Enter username and password
                string username, password;
                cout << "Enter username:\n";
                cin >> username;
                SSL_write(ssl, username.c_str(), username.size());
                cout << "Enter password:\n";
                cin >> password;
                SSL_write(ssl, password.c_str(), password.size());

                // Receive login message
                char login_msg[256] = {0};
                SSL_read(ssl, login_msg, sizeof(login_msg));
                cout << login_msg;

                string msg4 = "Welcome, " + username + "!\n";
                if(string(login_msg) == msg4){
                    login_username = username;
                    return 0;
                }
            }

        }else if(userInput == "3"){
            return -1;
        }else{
            cout << "Invalid option, try again!" << endl;
        }
    }

    return 0;
}

void sendFileInChunks(SSL* ssl, const string& filename) {
    int fileFD = open(filename.c_str(), O_RDONLY);
    if (fileFD == -1) {
        perror("Failed to open file");
        return;
    }

    char buffer[1024];
    ssize_t bytesRead;

    while ((bytesRead = read(fileFD, buffer, sizeof(buffer))) > 0) {
        // Send the file of chunk
        uint32_t chunkSize = htonl(bytesRead);
        if (SSL_write(ssl, &chunkSize, sizeof(chunkSize)) <= 0) {
            ERR_print_errors_fp(stderr);
            break;
        }

        // Send the data of chunk
        if (SSL_write(ssl, buffer, bytesRead) <= 0) {
            ERR_print_errors_fp(stderr);
            break;
        }
    }

    // Send a 0-byte chunk to indicate the end of the file
    uint32_t endChunk = 0;
    SSL_write(ssl, &endChunk, sizeof(endChunk));

    close(fileFD);
    // cout << "File sent in chunks successfully." << endl;
}

void receiveFileInChunks(SSL* ssl, string& filename) {
    filename = "received_" + filename;
    int fileFD = open(filename.c_str(), O_WRONLY | O_CREAT, 0666);
    if (fileFD == -1) {
        perror("Failed to create file");
        return;
    }

    char buffer[1024];
    uint32_t chunkSize;

    while (true) {
        // Receive the size of chunk
        if (SSL_read(ssl, &chunkSize, sizeof(chunkSize)) <= 0) {
            ERR_print_errors_fp(stderr);
            break;
        }
        chunkSize = ntohl(chunkSize);

        // If the size of chunk is 0, the transmission is over
        if (chunkSize == 0) {
            break;
        }

        // Receive the data of chunk
        ssize_t bytesReceived = SSL_read(ssl, buffer, chunkSize);
        if (bytesReceived > 0) {
            write(fileFD, buffer, bytesReceived);
        } else {
            ERR_print_errors_fp(stderr);
            break;
        }
    }

    close(fileFD);
    // cout << "File received in chunks successfully." << endl;
}

void checkForMessages(SSL* ssl) {
    fd_set readfds;
    struct timeval timeout;

    // Clear the file descriptor set
    FD_ZERO(&readfds);
    // Get the socket bound to SSL
    int clientSocket = SSL_get_fd(ssl);
    FD_SET(clientSocket, &readfds);

    // Set the timeout
    timeout.tv_sec = 2;  // Set 2 seconds timeout
    timeout.tv_usec = 0;

    // Check if there is data to read
    int ret = select(clientSocket + 1, &readfds, NULL, NULL, &timeout);

    if (ret == -1) {
        perror("select failed");
    } else if (ret == 0) {
        cout << "No message received." << endl;
    } else {
        if (FD_ISSET(clientSocket, &readfds)) {
            // There exists data to read
            char msgClientA[64] = {0}; // Set 50 for limiting the maximum length of the message, in case reading file date
            int bytesRead = SSL_read(ssl, msgClientA, sizeof(msgClientA));

            if (bytesRead > 0 && strncmp(msgClientA, "File", 4) == 0){
                cout << msgClientA << endl;
                size_t last_space = string(msgClientA).find_last_of(' ');
                string last_word = string(msgClientA).substr(last_space + 1);
                receiveFileInChunks(ssl, last_word);

            } else if (bytesRead > 0) {
                cout << msgClientA << endl;
            } else {
                cout << "Connection closed or no message." << endl;
            }
        }
    }
}

void receiveAndPlayAudioFrames(SSL* ssl) {
    ALCdevice* device = alcOpenDevice(nullptr);
    if (!device) {
        cerr << "Failed to open audio device." << endl;
        exit(EXIT_FAILURE);
    }

    ALCcontext* context = alcCreateContext(device, nullptr);
    alcMakeContextCurrent(context);

    ALuint source;
    ALuint buffers[BUFFER_COUNT];
    alGenSources(1, &source);
    alGenBuffers(BUFFER_COUNT, buffers);

    char frameBuffer[FRAME_SIZE];
    int bufferIndex = 0;

    // Fill the buffers with audio frames
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        int bytesRead = SSL_read(ssl, frameBuffer, FRAME_SIZE);
        if (bytesRead <= 0) {
            ERR_print_errors_fp(stderr);
            return;
        }

        alBufferData(buffers[i], AL_FORMAT_MONO16, frameBuffer, bytesRead, SAMPLING_RATE);
        alSourceQueueBuffers(source, 1, &buffers[i]);
    }

    alSourcePlay(source);

    while (true) {
        ALint processed = 0;
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
        // cout << "original processed: " << processed << endl;

        while (processed > 0) {
            
            ALuint buffer;
            alSourceUnqueueBuffers(source, 1, &buffer);
            cout << "new processed: " << processed << endl;

            fd_set readfds;
            struct timeval timeout;

            // Clear the file descriptor set
            FD_ZERO(&readfds);

            // Get the socket bound to SSL
            int clientSocket = SSL_get_fd(ssl);
            FD_SET(clientSocket, &readfds);

            // Set the timeout
            timeout.tv_sec = 1;  // Set 1 seconds timeout
            timeout.tv_usec = 0;

            // Check if there is data to read
            int ret = select(clientSocket + 1, &readfds, NULL, NULL, &timeout);

            if (ret == -1) {
                perror("select failed");
            } else if (ret == 0) {
                goto cleanup;
            }

            int bytesRead;
            if (FD_ISSET(clientSocket, &readfds)) { 
                // there exists data to read
                bytesRead = SSL_read(ssl, frameBuffer, FRAME_SIZE);
            }else{
                goto cleanup;
            }
            
            // int bytesRead = SSL_read(ssl, frameBuffer, FRAME_SIZE);
            if (bytesRead <= 0) {
                cout << "Connection closed by server or error occurred." << endl;
                goto cleanup;
            }

            alBufferData(buffer, AL_FORMAT_MONO16, frameBuffer, bytesRead, SAMPLING_RATE);
            alSourceQueueBuffers(source, 1, &buffer);
            --processed;
        }

        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            alSourcePlay(source); // Resume playback
        }

        usleep(1000); // Reduce CPU usage
    }
cleanup:
    alSourceStop(source);
    alDeleteSources(1, &source);
    alDeleteBuffers(BUFFER_COUNT, buffers);
    alcDestroyContext(context);
    alcCloseDevice(device);
}

void checkForStreaming(SSL* ssl) {
    fd_set readfds;
    int clientSocket = SSL_get_fd(ssl);

    int max_fd = (clientSocket > STDIN_FILENO) ? clientSocket : STDIN_FILENO;

    while(1){
        // Clear the file descriptor set
        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds); // Add the client socket to the set
        FD_SET(STDIN_FILENO, &readfds); // Add the standard input to the set

        // Check if there is data to read
        int ret = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (ret == -1) {
            perror("select failed");
            break;
        } 

        if (FD_ISSET(clientSocket, &readfds)) { 
            // There exists data to read
            char msgClientA[64] = {0}; // Set 50 for limiting the maximum length of the message, in case reading file date
            int bytesRead = SSL_read(ssl, msgClientA, sizeof(msgClientA));

            if (bytesRead > 0){
                cout << msgClientA << endl;
                receiveAndPlayAudioFrames(ssl);
                break;
            } else if (bytesRead = 0) {
                cout << "Connection closed or no message." << endl;
                break;
            } else {
                perror("read from socket error");
                break;
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[128];
            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (strcmp(input, "\n") == 0) {
                    printf("Exit triggered by Enter key.\n");
                    break;
                } else {
                    continue;
                }
            }
        }
    }
}

int main()
{   
    // Initialize OpenSSL
    initializeOpenSSL();
    SSL_CTX* ctx = createClientContext();

    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);

    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // sending connection request
    int ret = connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (ret==-1){
        perror("Failed to connect to server");
        exit(0);
    }

    // create SSL instance and bind to the client socket
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    int result = LogInProcess(ssl);

    char localIP[INET_ADDRSTRLEN];
    int localPort;
    // if login successfully, send login username, IP address and port to the server
    if(result != -1){
        // get the IP address and port of the client
        sockaddr_in localAddress;
        socklen_t addressLength = sizeof(localAddress);
        getsockname(clientSocket, (struct sockaddr*)&localAddress, &addressLength);
        
        inet_ntop(AF_INET, &localAddress.sin_addr, localIP, INET_ADDRSTRLEN);
        localPort = ntohs(localAddress.sin_port);

        // send the client information to the server
        string registerCommand = "CLIENT " + login_username + " " + localIP + " " + to_string(localPort);
        SSL_write(ssl, registerCommand.c_str(), registerCommand.size());
    }
    
    cin.clear();
    cin.ignore(); // clean the buffer

    while(result!=-1){
        cout << "Please enter your command: " << endl; 

        string msg = "";
        getline(cin, msg);
        // cout << "After getline(): [" << msg << "]" << endl;

        SSL_write(ssl, msg.c_str(), msg.size());

        if( msg == "logout" ){
            cout << "You have logged out.\n";
            result = LogInProcess(ssl);

        }else if( msg == "check msg" ){
            checkForMessages(ssl);

        }else if( msg == "check streaming" ){
            checkForStreaming(ssl);

        }else if( strncmp(msg.c_str(), "file", 4) == 0 ){
            size_t last_space = msg.find_last_of(' ');
            string filename = msg.substr(last_space + 1);
            sendFileInChunks(ssl, filename);

        }else if( strncmp(msg.c_str(), "streaming", 8) == 0 ){
            size_t last_space = msg.find_last_of(' ');
            string filename = msg.substr(last_space + 1);
            sendFileInChunks(ssl, filename);

        }else if( strncmp(msg.c_str(), "send", 4) != 0 ){
            cout << "No such command!" << endl;
        }
    }

    cout << "quit" << endl;

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(clientSocket);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    return 0;
}
