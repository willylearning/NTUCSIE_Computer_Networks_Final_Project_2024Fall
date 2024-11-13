// reference : https://www.geeksforgeeks.org/socket-programming-in-cpp/ 
#include <cstring>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include "common.h"

using namespace std;

unordered_map<string, string> userdata;


int LogInCheck(int fd){
    // write the file desciptor into the client
    const char *msg = "1.Register  2.Login  3.Quit : ";
    write(fd, msg , strlen(msg));

    // while loop for registration and login/logout
    while(true)
    {
        // recieving data
        char response[256] = {0};
        read(fd, response, sizeof(response));
        // cout << response;
        
        if(strcmp(response, "1") == 0){ // registration
            const char *msg1 = "Enter Username: ";
            write(fd, msg1, strlen(msg1));

            char new_username[256] = {0};
            read(fd, new_username, sizeof(new_username));
            cout << "reg username received: " << new_username << endl;

            const char *msg2 = "Create Password: ";
            write(fd, msg2, strlen(msg2));

            char new_password[256] = { 0 };
            read(fd, new_password, sizeof(new_password));
            cout << "reg password received: " << new_password << endl;

            // save the username and corresponding password into userdata map
            userdata[string(new_username)] = string(new_password);
            const char *msg3 = "You have registered successfully!\n1.Register  2.Login  3.Quit : ";
            write(fd, msg3, strlen(msg3));
            cout << "registered successfully.\n";
        }
        else if(strcmp(response, "2") == 0){ // login
            const char *msg1 = "Enter Username: ";
            write(fd, msg1, strlen(msg1));
            string correct_username;
            while(true){
                char login_username[256] = {0};
                read(fd, login_username, sizeof(login_username));
                cout << "login username received: " << login_username << endl;

                // wrong username case
                if(userdata.find(login_username) == userdata.end()){
                    const char *msg2 = "Username not found, please try again.\n";
                    write(fd, msg2, strlen(msg2));
                }else{
                    correct_username = string(login_username);
                    break;
                }
            }
            
            const char *msg3 = "Enter Password: ";
            write(fd, msg3, strlen(msg3));

            while(true)
            {
                char login_password[256] = {0};
                read(fd, login_password, sizeof(login_password));
                cout << "login password received: " << login_password << endl;

                // wrong password case  //ToDO: retry count?
                if(string(login_password) != userdata[correct_username]){ //ToDO: compared by hash?
                    const char *msg = "Wrong password, please try again.\n";
                    write(fd, msg, strlen(msg));
                }else{
                    break;
                }
            }
            const char *msg4 = "Welcome!";
            write(fd, msg4, strlen(msg4));

            return 0;
        }
        else if (strcmp(response, "3") == 0){
            return -1;
        }
    }

    return 0;
}

int main(int argc , char *argv[])
{
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(serverSocket, (struct sockaddr*)&serverAddress,
         sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    // accepting connection request
    int clientSocket = accept(serverSocket, nullptr, nullptr); // a file descriptor
    cout << "server connected.\n";
  
    int result = LogInCheck(clientSocket);
    cout << "cj2" << endl;
    
    while(result!=-1){
        // receive the message "logout" from client
        char msg[256] = {0};
        read(clientSocket, msg, sizeof(msg));
        cout << "C> " << msg << endl;
        if(strcmp(msg, "logout") == 0){
            write(clientSocket, "You have logged out.\n", strlen("You have logged out.\n"));
            break;
        }

        string msg2;
        cout << "S> "; cin >> msg2;
        write(clientSocket, msg2.c_str(), msg2.size());
    }
    
    // closing the socket.
    close(serverSocket);

    return 0;
}