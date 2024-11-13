//
// reference : https://www.geeksforgeeks.org/socket-programming-in-cpp/
// C++ program to illustrate the client application in the
// socket programming
#include <cstring>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common.h"

using namespace std;

int LogInProcess(int fd)
{
    // cout << "client test\n";
    // receive the Logging In message
    char buffer[256] = {0};
    read(fd, buffer, sizeof(buffer));
    cout << buffer;

    while(true)
    {
        // type r or l
        string userInput;
        cin >> userInput;
        // write(clientSocket, userInput.c_str(), userInput.size());
        write(fd, userInput.c_str(), userInput.size());

        if (userInput == "1") { // register 
        #if 0  //ToDO: client program initiates user/password prompts 

        #else
        #endif
            // cout << "rtest\n";
            // receive the message "Set your username:\n"
            char reg_username_prompt[256] = {0};
            read(fd, reg_username_prompt, sizeof(reg_username_prompt));
            cout << reg_username_prompt;

            // set the username
            string username;
            cin >> username;
            write(fd, username.c_str(), username.size());

            // receive the message "Set your password:\n"
            char reg_password_prompt[256] = {0};
            read(fd, reg_password_prompt, sizeof(reg_password_prompt));
            cout << reg_password_prompt;

            // set the password
            string password;
            cin >> password;
            write(fd, password.c_str(), password.size());

            // receive the message "You have registered successfully!\n"
            char reg_success_prompt[256] = {0};
            read(fd, reg_success_prompt, sizeof(reg_success_prompt));
            cout << reg_success_prompt;

        }else if(userInput == "2"){ // logging in
            // cout << "ltest\n";
            // receive the message "Enter your username:\n"
            char login_username_prompt[256] = {0};
            read(fd, login_username_prompt, sizeof(login_username_prompt));
            cout << login_username_prompt;

            while(true){
                // enter username
                string username;
                cin >> username;
                write(fd, username.c_str(), username.size());

                // receive the message "Enter your password:\n" or "Username not found, please try again.\n"
                char login_password_prompt[256] = {0};
                read(fd, login_password_prompt, sizeof(login_password_prompt));
                cout << login_password_prompt;
                if(strcmp(login_password_prompt, "Enter Password: ") == 0){
                    cout << "test ";
                    break;
                }
            }

            while(true){
                // enter password
                string password;
                cin >> password;
                write(fd, password.c_str(), password.size());

                // receive the message "Welcome!\n" or "Wrong password, please try again.\n"
                char login_success_prompt[256] = {0};
                read(fd, login_success_prompt, sizeof(login_success_prompt));

                cout << login_success_prompt << endl;
                if(strcmp(login_success_prompt, "Welcome!") == 0){
                    cout << "cj o" << endl;
                    return 0;
                }
                else 
                    cout << "cj1 x" << endl;
                
                //return 0;
            }
            //break;
        }
        else if(userInput == "3"){
            return -1;
        }
        else{
            cout << "Invalid option, try again!" << endl;
        }
    }

    return 0;
}

int main()
{
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
#if 1
    serverAddress.sin_addr.s_addr = INADDR_ANY;
#else
    //serverAddress.sin_addr.s_addr = inet_addr("192.168.1.122"); //INADDR_ANY;
    //struct in_addr addr;
    int result = inet_pton(AF_INET, "192.168.1.122", &serverAddress.sin_addr.s_addr );
    cout << "result:" << result << endl;
    if (result == 1) {
        // Address conversion successful
        // Use addr.s_addr for the binary representation
        
    } else {
        // Address conversion failed
        // Handle the error appropriately
    }
#endif

    // sending connection request
    int ret = connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (ret==-1){
        perror("connect");
        exit(0);
    }

    int result = LogInProcess(clientSocket);
    cout << "cj3" << endl;
    while(result!=-1){
        string msg;
        cout << "C> "; cin >> msg;
        write(clientSocket, msg.c_str(), msg.size());
        if(msg == "logout"){
            char logout_prompt[256] = {0};
            write(clientSocket, logout_prompt, sizeof(logout_prompt));
            cout << logout_prompt;
            break;
        }

        char msgS[256] = {0};
        read(clientSocket, msgS, sizeof(msg));
        cout << "S> " << msgS << endl;
    }

    // closing socket
    close(clientSocket);

    return 0;
}
