#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <queue>
#include <pthread.h>
#include <signal.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <fcntl.h>

using namespace std;

// unordered_map<string, int> onlineClients;
unordered_map<string, SSL*> onlineClients;
pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

unordered_map<string, pair<string, int>> clientInfo;
pthread_mutex_t clientInfoMutex = PTHREAD_MUTEX_INITIALIZER; // 保護 clientInfo 的互斥鎖

unordered_map<string, string> userdata;

queue<int> clientQueue;
pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueCond = PTHREAD_COND_INITIALIZER;

// Setting for audio streaming 
#define PORT 8080
#define FRAME_SIZE 65536 

struct Frame {
    uint32_t frameID;         
    char data[FRAME_SIZE];    
    uint32_t dataSize;        
};


void initializeOpenSSL() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

SSL_CTX* createServerContext() {
    const SSL_METHOD* method = SSLv23_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configureServerContext(SSL_CTX* ctx) {
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void handleSignal(int signum) {
    cout << "\nLogout not allowed via direct interruption. Please use the logout command.\n";
}

int LogInCheck(SSL* ssl){
    // signal(SIGINT, handleSignal);

    // while loop for registration and login/logout
    while(true)
    {
        // recieving the option from client
        char response[256] = {0};
        SSL_read(ssl, response, sizeof(response));
        // cout << response;
        
        if (strcmp(response, "1") == 0){ // registration
            char new_username[256] = {0};
            SSL_read(ssl, new_username, sizeof(new_username));
            cout << "reg username received: " << new_username << endl;

            char new_password[256] = { 0 };
            SSL_read(ssl, new_password, sizeof(new_password));
            cout << "reg password received: " << new_password << endl;

            // save the username and corresponding password into userdata map
            userdata[string(new_username)] = string(new_password);

        }else if (strcmp(response, "2") == 0){ // login
            while(true){
                char login_username[256] = {0};
                SSL_read(ssl, login_username, sizeof(login_username));
                cout << "login username received: " << login_username << endl;
                char login_password[256] = {0};
                SSL_read(ssl, login_password, sizeof(login_password));
                cout << "login password received: " << login_password << endl;

                // login condition
                if(userdata.find(string(login_username)) == userdata.end()){ // wrong username case
                    const char *msg2 = "Username not found, please try again.\n";
                    SSL_write(ssl, msg2, strlen(msg2));
                }else if(string(login_password) != userdata[string(login_username)]){ // correct username but wrong password
                    const char *msg3 = "Wrong password, please try again.\n";
                    SSL_write(ssl, msg3, strlen(msg3));
                }else{ // both correct
                    // char msg4[300] = "Welcome!\n"; 
                    string msg4 = "Welcome, " + string(login_username) + "!\n";
                    SSL_write(ssl, msg4.c_str(), msg4.size());
                    return 0;
                }
            }
        }else if (strcmp(response, "3") == 0){
            return -1;
        }
    }

    return 0;
}

void receiveFileInChunks(SSL* ssl, string& filename) {
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
            cout << buffer << endl;
        } else {
            ERR_print_errors_fp(stderr);
            break;
        }
    }

    close(fileFD);
    cout << "Server has received file in chunks successfully." << endl;
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
    cout << "Server has sent file in chunks successfully." << endl;
}

void streamAudioFrames(SSL* ssl, const string& filename) {
    std::ifstream audioFile(filename, std::ios::binary);
    if (!audioFile.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    Frame frame;
    uint32_t frameID = 0;

    while (audioFile.read(frame.data, FRAME_SIZE) || audioFile.gcount() > 0) {
        frame.frameID = frameID++;
        frame.dataSize = audioFile.gcount();
        if (SSL_write(ssl, &frame, sizeof(frame)) <= 0) {
            ERR_print_errors_fp(stderr);
            break;
        }
        usleep(23000); // 23ms
    }

    audioFile.close();
    cout << "Audio streaming finished." << endl;
}


// Worker Thread function
void* workerThread(void* arg) {
    SSL_CTX* ctx = (SSL_CTX*)arg;
    int clientSocket;
    // int clientSocket = *(int*)arg;

    // Get one client socket from the queue
    pthread_mutex_lock(&queueMutex);
    while (clientQueue.empty()) {
        pthread_cond_wait(&queueCond, &queueMutex);  // Wait until there is a new connection
    }
    clientSocket = clientQueue.front();
    clientQueue.pop();
    pthread_mutex_unlock(&queueMutex);

    // Create SSL instance and bind to the client socket
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);
    
    // SSL accept
    if (SSL_accept(ssl) <= 0) {
        return nullptr;
    }

    int result = LogInCheck(ssl);

    if (result != -1) {
        // receive login username
        char msgClientInfo[256] = {0};
        SSL_read(ssl, msgClientInfo, sizeof(msgClientInfo));
        cout << msgClientInfo << endl;

        // CLIENT <username> <IP> <Port>
        if(strncmp(msgClientInfo, "CLIENT", 6) == 0){
            string message(msgClientInfo);
            size_t firstSpace = message.find(' ');
            size_t secondSpace = message.find(' ', firstSpace + 1);
            size_t thirdSpace = message.find(' ', secondSpace + 1);

            string clientName = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            string clientIP = message.substr(secondSpace + 1, thirdSpace - secondSpace - 1);
            int clientPort = stoi(message.substr(thirdSpace + 1));

            // Add clients' ssl socket to onlineClients
            pthread_mutex_lock(&clientMutex);
            onlineClients[clientName] = ssl;
            pthread_mutex_unlock(&clientMutex);

            // Save client's IP and port to clientInfo
            pthread_mutex_lock(&clientInfoMutex);
            clientInfo[clientName] = {clientIP, clientPort};
            cout << "Registered client: " << clientName << " IP: " << clientInfo[clientName].first << " Port: " << clientInfo[clientName].second 
                    << " clientSocket: " << clientSocket << endl;
            pthread_mutex_unlock(&clientInfoMutex);
        }
    }

    while (result != -1) {
        char msgC[256] = {0};

        SSL_read(ssl, msgC, sizeof(msgC));

        if ( strcmp(msgC, "logout") == 0 ) {
            result = LogInCheck(ssl);
        }else if ( strncmp(msgC, "send", 4) == 0 ){ // <send kevin james hello> means that kevin wants to send message "hello" to james
            string SendRequest(msgC);
            size_t firstSpace = SendRequest.find(' ');
            size_t secondSpace = SendRequest.find(' ', firstSpace + 1);
            size_t thirdSpace = SendRequest.find(' ', secondSpace + 1);

            string clientA = SendRequest.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            string clientB = SendRequest.substr(secondSpace + 1, thirdSpace - secondSpace - 1);
            string AtoBMessage = SendRequest.substr(thirdSpace + 1);

            cout << "Send message from " << clientA << " to " << clientB << ": " << AtoBMessage << endl;

            if (clientInfo.find(clientB) != clientInfo.end()) {
                // Server gets james's IP and port by using clientInfo[clientB]
                string clientB_IP = clientInfo[clientB].first;
                int clientB_Port = clientInfo[clientB].second;
                // cout << "ClientB IP: " << clientB_IP << " Port: " << clientB_Port << endl;

                // int targetSocket;
                SSL* targetSSL = nullptr;

                pthread_mutex_lock(&clientMutex);
                if (onlineClients.find(clientB) != onlineClients.end()) {
                    // Successfully find the socket of the target client
                    targetSSL = (SSL*)onlineClients[clientB]; 
                } else {
                    cout << "Target client not online." << endl;
                    pthread_mutex_unlock(&clientMutex);
                    continue;
                }
                pthread_mutex_unlock(&clientMutex);

                // Send the message to clientB
                string relayMessage = "Message from " + clientA + ": " + AtoBMessage;
                SSL_write(targetSSL, relayMessage.c_str(), relayMessage.size());

            }else {
                // Target client not found
                string errorMsg = "Error: Client " + clientB + " not found\n";
                SSL_write(ssl, errorMsg.c_str(), errorMsg.size());
            }
        }else if (strncmp(msgC, "file", 4) == 0){ // <file kevin james hw3.pdf> means that kevin wants to send hw3.pdf to james
            string SendRequest(msgC);
            size_t firstSpace = SendRequest.find(' ');
            size_t secondSpace = SendRequest.find(' ', firstSpace + 1);
            size_t thirdSpace = SendRequest.find(' ', secondSpace + 1);

            string clientA = SendRequest.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            string clientB = SendRequest.substr(secondSpace + 1, thirdSpace - secondSpace - 1);
            string filename = SendRequest.substr(thirdSpace + 1);
            receiveFileInChunks(ssl, filename);

            cout << "Send file from " << clientA << " to " << clientB << ": " << filename << endl;

            if (clientInfo.find(clientB) != clientInfo.end()) {
                // Server gets james's IP and port by using clientInfo[clientB]
                string clientB_IP = clientInfo[clientB].first;
                int clientB_Port = clientInfo[clientB].second;
                // cout << "ClientB IP: " << clientB_IP << " Port: " << clientB_Port << endl;

                // int targetSocket;
                SSL* targetSSL = nullptr;

                pthread_mutex_lock(&clientMutex);
                if (onlineClients.find(clientB) != onlineClients.end()) {
                    targetSSL = (SSL*)onlineClients[clientB]; // Find the socket of the target client
                } else {
                    cout << "Target client not online." << endl;
                    pthread_mutex_unlock(&clientMutex);
                    continue;
                }
                pthread_mutex_unlock(&clientMutex);

                // Send the message to clientB
                string relayMessage = "File from " + clientA + ": " + filename;
                SSL_write(targetSSL, relayMessage.c_str(), relayMessage.size());

                sendFileInChunks(targetSSL, filename);

            }else {
                // Target client not found
                string errorMsg = "Error: Client " + clientB + " not found\n";
                SSL_write(ssl, errorMsg.c_str(), errorMsg.size());
            }
        }else if (strncmp(msgC, "streaming", 8) == 0){ // <streaming kevin james s60.wav> means that kevin wants to stream audio to james
            string SendRequest(msgC);
            size_t firstSpace = SendRequest.find(' ');
            size_t secondSpace = SendRequest.find(' ', firstSpace + 1);
            size_t thirdSpace = SendRequest.find(' ', secondSpace + 1);

            string clientA = SendRequest.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            string clientB = SendRequest.substr(secondSpace + 1, thirdSpace - secondSpace - 1);
            string filename = SendRequest.substr(thirdSpace + 1);
            receiveFileInChunks(ssl, filename);

            cout << "Streaming from " << clientA << " to " << clientB << ": " << filename << endl;

            if (clientInfo.find(clientB) != clientInfo.end()) {
                // Server gets james's IP and port by using clientInfo[clientB]
                string clientB_IP = clientInfo[clientB].first;
                int clientB_Port = clientInfo[clientB].second;

                // int targetSocket;
                SSL* targetSSL = nullptr;

                pthread_mutex_lock(&clientMutex);
                if (onlineClients.find(clientB) != onlineClients.end()) {
                    targetSSL = (SSL*)onlineClients[clientB]; // Find the socket of the target client
                } else {
                    cout << "Target client not online." << endl;
                    pthread_mutex_unlock(&clientMutex);
                    continue;
                }
                pthread_mutex_unlock(&clientMutex);

                // Send the message to clientB
                string relayMessage = "Streaming from " + clientA + ": " + filename;
                SSL_write(targetSSL, relayMessage.c_str(), relayMessage.size());

                streamAudioFrames(targetSSL, filename);

            }else {
                // Target client not found
                string errorMsg = "Error: Client " + clientB + " not found\n";
                SSL_write(ssl, errorMsg.c_str(), errorMsg.size());
            }
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(clientSocket);

    return nullptr;
}

int main(int argc , char *argv[])
{
    // Initialize OpenSSL
    initializeOpenSSL();
    SSL_CTX* ctx = createServerContext();
    configureServerContext(ctx);

    // Creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Bind and listen
    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    listen(serverSocket, 10);

    cout << "Server listening on port 8080..." << endl;

    // Main loop, accept new connections
    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr); // return a file descriptor for the accepted socket
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        // Use workerThread for concurrent connections
        const int NUM_THREADS = 10;
        pthread_t threads[NUM_THREADS];
        for (int i = 0; i < NUM_THREADS; ++i) {
            // pthread_create(&threads[i], nullptr, workerThread, &(clientSocket));
            pthread_create(&threads[i], nullptr, workerThread, ctx);
        }

        // Add the new client socket to the queue
        pthread_mutex_lock(&queueMutex);
        clientQueue.push(clientSocket);
        pthread_cond_signal(&queueCond);  // Notify the worker threads
        pthread_mutex_unlock(&queueMutex);

    }

    // closing the socket.
    close(serverSocket);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}