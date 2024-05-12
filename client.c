#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <pthread.h>

#define LOGIN 0
#define LO_ACK 1
#define LO_NAK 2
#define EXIT 3
#define JOIN 4
#define JN_ACK 5
#define JN_NAK 6
#define LEAVE_SESS 7
#define NEW_SESS 8
#define NS_ACK 9
#define MESSAGE 10
#define QUERY 11
#define QU_ACK 12
#define EXIT_ACK 13
#define LEAVE_ACK 14
#define PRIV 15
#define REGISTER 16
#define PRIV_ACK 17
#define PRIV_NACK 18
#define REG_NAK 19

#define MAX_NAME 1024
#define MAX_DATA 1024

struct Message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

struct ThreadPass {
    int sockfd;
    char clientID[MAX_NAME];
    char serverIP[MAX_NAME];
};

int calcdigits(int value) {
    int count = 0; 
    
    while (value >= 10) {
        value = value/10;
        count++;
    }
    return count + 1;
}

//global flags
bool globalLogout = false;
bool globalQuit = false;
bool globalLeave = false;
bool globalJoin = false;
bool globalSess = false;

void printList(char list[]) {
    char sessName[MAX_NAME];
    char userName[MAX_NAME];
    bool addSess = true;
    int sessIndex = 0;
    int userIndex = 0;


    for (int i = 0; i < strlen(list); i++) {
        if (addSess) {
            if (list[i] == '-') {
                sessName[sessIndex] = '\0';
                sessIndex = 0;
                addSess = false; 
                printf("Session Name: %s\nUsers:\n", sessName);
            } else {
                sessName[sessIndex] = list[i];
                sessIndex++;
            }
        } else {
            if (list[i] == '|') {
                addSess = true;
                if (i != strlen(list) - 1) {
                    printf("\n");
                }
            } else if (list[i] == ',') {
                userName[userIndex] = '\0';
                userIndex = 0;
                printf("%s\n", userName);
            } else {
                userName[userIndex] = list[i];
                userIndex++;
            }
        }
    }
}

//receiving messages from server
void receiveText(int sockfdpassed) {
    int numbytes;
    char bufrec[10000];
    int sockfd = sockfdpassed;

    while (true) {

        if ( (numbytes = recv(sockfd, bufrec, 10000, 0)) == -1)
        {   
            if (errno == ECONNRESET || errno == ECONNABORTED || errno == ENOTCONN) {
                printf("Server Disconnected\n");
                globalQuit = true;
                break;
            }
            perror("accept");
            exit(1);
        }

        if (numbytes == 0) {
            printf("Server Disconnected\n");
            globalQuit = true;
            break;
        }

        bufrec[numbytes] = '\0';

        if (bufrec[0] == '1' && bufrec[1] == '0') {
            char text[MAX_DATA];
            int count = 0;
            int index_save = 0;
            for (int i = 0; i < strlen(bufrec); i++) {
                if (i == strlen(bufrec) - 1) {
                    text[i - index_save] = '\0';
                } else {
                    if (count >= 3) {
                        text[i - index_save] = bufrec[i];
                    }
                    if (bufrec[i] == ':') {
                        count++;
                        if (count == 3) {
                            index_save = i + 1;
                        }
                    }
                }
            }

            printf("%s\n", text);
        } else if (bufrec[0] == '1' && bufrec[1] == '2') {
            char data[MAX_DATA];
            int count = 0;
            int index_save = 0;
            for (int i = 0; i < strlen(bufrec); i++) {
                if (i == strlen(bufrec) - 1) {
                    data[i - index_save] = '\0';
                } else {
                    if (count >= 3) {
                        data[i - index_save] = bufrec[i];
                    }
                    if (bufrec[i] == ':') {
                        count++;
                        if (count == 3) {
                            index_save = i + 1;
                        }
                    }
                }
            }
            if (strlen(data) == 0) {
                printf("No ongoing sessions\n");
            } else {
                printList(data);
            }
        } else if (bufrec[0] == '1' && bufrec[1] == '3') {
            //printf("Exit ACK Recieved\n");
        } else if (bufrec[0] == '1' && bufrec[1] == '4') {
            //printf("Leave ACK Received\n");
        } else if (bufrec[0] == '1' && bufrec[1] == '8') {
            printf("Username not found\n");
        } else if (bufrec[0] == '1' && bufrec[1] == '5') {
            char text[MAX_DATA];
            int count = 0;
            int index_save = 0;
            for (int i = 0; i < strlen(bufrec); i++) {
                if (i == strlen(bufrec) - 1) {
                    text[i - index_save] = '\0';
                } else {
                    if (count >= 3) {
                        text[i - index_save] = bufrec[i];
                    }
                    if (bufrec[i] == ':') {
                        count++;
                        if (count == 3) {
                            index_save = i + 1;
                        }
                    }
                }
            }

            printf("%s\n", text);
        } else if (bufrec[0] == '6') {
            printf("Join Failed\n");
            globalJoin = true;
        } else if (bufrec[0] == '5' || bufrec[0] == '9') {
            globalSess = true;
            break;
        } else if (bufrec[0] == '1' && bufrec[1] == '7') {
            //Successful Private Messaging
        } else {
            printf("Type not recognized\n");
            printf("%s\n", bufrec);
        }

        if (globalLeave || globalLogout || globalQuit || globalJoin) {
            break;
        }

    }
}

//state of joining session
void secondState(void* args) {
    int numbytes;
    bool flag;
    char clientID[MAX_NAME];
    char serverIP[MAX_NAME];
    struct Message message;
    struct ThreadPass *param = (struct ThreadPass*) args;

    int sockfd = param->sockfd;
    sprintf(clientID, param->clientID);
    sprintf(serverIP, param->serverIP);

    while (true) {
        char input[2048];

        fgets(input, sizeof(input), stdin);
        int newline_position = strcspn(input, "\n");
        input[newline_position] = '\0';

        char command[30];

        int index_save;

        flag = false;
        for (int i = 0; i < 30; i++) {
            if (i == 0) {
                if (input[i] == '\0') {
                    printf("No input given\n");
                    flag = true;
                    break;
                }

                if (input[i] != '/') {
                    index_save = i;
                    sprintf(command, "text");
                    break;
                }
            } else {
                if (input[i] != ' ' && input[i] != '\0') {
                    command[i - 1] = input[i];
                } else {
                    index_save = i;
                    command[i - 1] = '\0';
                    break;
                }
            }
        }
        if (flag) {
            continue;
        }

        for (int i = 0; i < MAX_DATA; i++) {
            message.data[i] = '\0';
        }

        if (strcmp(command, "joinsession") == 0) {

            if (strcmp(clientID, "None~") == 0) {
                printf("Not logged in\n");
                continue;
            }

            message.type = JOIN;

            if (input[index_save] == '\0') {
                printf("No session id found\n");
                continue;
            } else if (input[index_save + 1] == '\0') {
                printf("No session id found\n");
                continue;
            }

            for (int i = index_save + 1; i < 1050; i++) {
                if (input[i] != '\0') {
                    message.data[i - index_save - 1] = input[i];
                } else {
                    message.data[i - index_save - 1] = '~';
                    message.data[i - index_save] = '\0';
                    break;
                }
            }

            sprintf(message.source, clientID);

            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
            break;
        } else if (strcmp(command, "createsession") == 0) {
            if (strcmp(clientID, "None~") == 0) {
                printf("Not logged in\n");
                continue;
            }

            message.type = NEW_SESS;

            if (input[index_save] == '\0') {
                printf("No session id found\n");
                continue;
            } else if (input[index_save + 1] == '\0') {
                printf("No session id found\n");
                continue;
            }

            for (int i = index_save + 1; i < 1050; i++) {
                if (input[i] != '\0') {
                    message.data[i - index_save - 1] = input[i];
                } else {
                    message.data[i - index_save - 1] = '~';
                    message.data[i - index_save] = '\0';
                    break;
                }
            }

            sprintf(message.source, clientID);

            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
            break;
        } else if (strcmp(command, "list") == 0) {
            message.type = QUERY;

            sprintf(message.data, "~");

            sprintf(message.source, clientID);

            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
        } else if (strcmp(command, "quit") == 0) {
            message.type = EXIT;
            sprintf(message.data, "~");
            sprintf(message.source, clientID);
            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;

            char sendMessTemp[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

            char str1temp[10];

            sprintf(sendMessTemp, "%d", message.type);
            strcat(sendMessTemp, ":");
            sprintf(str1temp, "%d", message.size);
            strcat(sendMessTemp, str1temp);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.source);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.data);

            if ((numbytes = send(sockfd, (const void *)sendMessTemp, strlen(sendMessTemp), 0)) == -1) {
                perror("send");
                close(sockfd);
                exit(1);
            }

            globalQuit = true;
            return 0;
        } else if (strcmp(command, "logout") == 0) {
            message.type = EXIT;
            sprintf(message.data, "~");
            sprintf(message.source, clientID);
            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;

            char sendMessTemp[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

            char str1temp[10];

            sprintf(sendMessTemp, "%d", message.type);
            strcat(sendMessTemp, ":");
            sprintf(str1temp, "%d", message.size);
            strcat(sendMessTemp, str1temp);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.source);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.data);

            if ((numbytes = send(sockfd, (const void *)sendMessTemp, strlen(sendMessTemp), 0)) == -1) {
                perror("send");
                close(sockfd);
                exit(1);
            }

            globalLogout = true;
            return 0;
        } else if (strcmp(command, "priv") == 0) {
            if (strcmp(clientID, "None~") == 0) {
                printf("Not logged in\n");
                continue;
            }

            message.type = PRIV;

            if (input[index_save] == '\0') {
                printf("No user or message found\n");
                continue;
            } else if (input[index_save + 1] == '\0') {
                printf("No user or message found\n");
                continue;
            }

            bool plus_flag = false;

            for (int i = index_save + 1; i < 1050; i++) {
                if (input[i] == '+') {
                    plus_flag = true;
                }

                if (input[i] != '\0') {
                    message.data[i - index_save - 1] = input[i];
                } else {
                    if (input[i - 1] == '+') {
                        plus_flag = false;
                    }

                    message.data[i - index_save - 1] = '~';
                    message.data[i - index_save] = '\0';
                    break;
                }
            }

            if (!plus_flag) {
                printf("No message found\n");
                continue;
            }

            sprintf(message.source, clientID);

            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
        } else {
            printf("Error reading command\n");
            continue;
        }

        char sendMessTemp[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

        char str1temp[10];

        sprintf(sendMessTemp, "%d", message.type);
        strcat(sendMessTemp, ":");
        sprintf(str1temp, "%d", message.size);
        strcat(sendMessTemp, str1temp);
        strcat(sendMessTemp, ":");
        strcat(sendMessTemp, message.source);
        strcat(sendMessTemp, ":");
        strcat(sendMessTemp, message.data);

        if ((numbytes = send(sockfd, (const void *)sendMessTemp, strlen(sendMessTemp), 0)) == -1) {
            perror("send");
            close(sockfd);
            exit(1);
        }

    }

    char sendMess2[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

    char str2[10];

    sprintf(sendMess2, "%d", message.type);
    strcat(sendMess2, ":");
    sprintf(str2, "%d", message.size);
    strcat(sendMess2, str2);
    strcat(sendMess2, ":");
    strcat(sendMess2, message.source);
    strcat(sendMess2, ":");
    strcat(sendMess2, message.data);

    char createMess[strlen(sendMess2)];

    strcpy(createMess, sendMess2);

    if ((numbytes = send(sockfd, (const void *)createMess, strlen(createMess), 0)) == -1) {
        perror("send");
        close(sockfd);
        exit(1);
    }
}

//state of in session
void lastState(void* args) {
    int numbytes;
    bool flag;
    char clientID[MAX_NAME];
    char serverIP[MAX_NAME];
    struct Message message;
    struct ThreadPass *param = (struct ThreadPass*) args;

    int sockfd = param->sockfd;
    sprintf(clientID, param->clientID);
    sprintf(serverIP, param->serverIP);

    while (true) {
        char input[2048];

        fgets(input, sizeof(input), stdin);
        int newline_position = strcspn(input, "\n");
        input[newline_position] = '\0';

        char command[30];

        int index_save;

        flag = false;
        for (int i = 0; i < 30; i++) {
            if (i == 0) {
                if (input[i] == '\0') {
                    printf("No input given\n");
                    flag = true;
                    break;
                }

                if (input[i] != '/') {
                    index_save = i;
                    sprintf(command, "text");
                    break;
                }
            } else {
                if (input[i] != ' ' && input[i] != '\0') {
                    command[i - 1] = input[i];
                } else {
                    index_save = i;
                    command[i - 1] = '\0';
                    break;
                }
            }
        }
        if (flag) {
            continue;
        }

        for (int i = 0; i < MAX_DATA; i++) {
            message.data[i] = '\0';
        }

        if (strcmp(command, "leavesession") == 0) {
            message.type = LEAVE_SESS;
            sprintf(message.data, "~");
            sprintf(message.source, clientID);
            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
            globalLeave = true;

            if (globalQuit) {
                break;
            }

            char sendMessTemp[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

            char str1temp[10];

            sprintf(sendMessTemp, "%d", message.type);
            strcat(sendMessTemp, ":");
            sprintf(str1temp, "%d", message.size);
            strcat(sendMessTemp, str1temp);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.source);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.data);

            if ((numbytes = send(sockfd, (const void *)sendMessTemp, strlen(sendMessTemp), 0)) == -1) {
                perror("send");
                close(sockfd);
                exit(1);
            }

            break;
        } else if (strcmp(command, "text") == 0) {
            message.type = MESSAGE;

            for (int i = index_save; i <= 1050; i++) {
                if (input[i] != '\0') {
                    message.data[i] = input[i];
                } else {
                    message.data[i] = '~';
                    message.data[i + 1] = '\0';
                    break;
                }
            }
            
            sprintf(message.source, clientID);

            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
        } else if (strcmp(command, "quit") == 0) {
            message.type = EXIT;
            sprintf(message.data, "~");
            sprintf(message.source, clientID);
            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;

            if (globalQuit) {
                break;
            }

            char sendMessTemp[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

            char str1temp[10];

            sprintf(sendMessTemp, "%d", message.type);
            strcat(sendMessTemp, ":");
            sprintf(str1temp, "%d", message.size);
            strcat(sendMessTemp, str1temp);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.source);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.data);

            if ((numbytes = send(sockfd, (const void *)sendMessTemp, strlen(sendMessTemp), 0)) == -1) {
                perror("send");
                close(sockfd);
                exit(1);
            }

            globalQuit = true;
            break;
        } else if (strcmp(command, "list") == 0) {
            message.type = QUERY;

            sprintf(message.data, "~");

            sprintf(message.source, clientID);

            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
        } else if (strcmp(command, "logout") == 0) {
            message.type = EXIT;
            sprintf(message.data, "~");
            sprintf(message.source, clientID);
            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;

            if (globalQuit) {
                break;
            }

            char sendMessTemp[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

            char str1temp[10];

            sprintf(sendMessTemp, "%d", message.type);
            strcat(sendMessTemp, ":");
            sprintf(str1temp, "%d", message.size);
            strcat(sendMessTemp, str1temp);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.source);
            strcat(sendMessTemp, ":");
            strcat(sendMessTemp, message.data);

            if ((numbytes = send(sockfd, (const void *)sendMessTemp, strlen(sendMessTemp), 0)) == -1) {
                perror("send");
                close(sockfd);
                exit(1);
            }

            globalLogout = true;
            break;
        } else if (strcmp(command, "priv") == 0) {
            if (strcmp(clientID, "None~") == 0) {
                printf("Not logged in\n");
                continue;
            }

            message.type = PRIV;

            if (input[index_save] == '\0') {
                printf("No user or message found\n");
                continue;
            } else if (input[index_save + 1] == '\0') {
                printf("No user or message found\n");
                continue;
            }

            bool plus_flag = false;

            for (int i = index_save + 1; i < 1050; i++) {
                if (input[i] == '+') {
                    plus_flag = true;
                }

                if (input[i] != '\0') {
                    message.data[i - index_save - 1] = input[i];
                } else {
                    if (input[i - 1] == '+') {
                        plus_flag = false;
                    }

                    message.data[i - index_save - 1] = '~';
                    message.data[i - index_save] = '\0';
                    break;
                }
            }

            if (!plus_flag) {
                printf("No message found\n");
                continue;
            }

            sprintf(message.source, clientID);

            message.size = strlen(message.data) + 4 + strlen(message.source);
            int num_digits = calcdigits(message.size);
            num_digits = calcdigits(message.size + num_digits);
            message.size += num_digits;
        } else {
            printf("Invalid Command given\n");
            continue;
        }

        char sendMessTemp[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

        char str1temp[10];

        sprintf(sendMessTemp, "%d", message.type);
        strcat(sendMessTemp, ":");
        sprintf(str1temp, "%d", message.size);
        strcat(sendMessTemp, str1temp);
        strcat(sendMessTemp, ":");
        strcat(sendMessTemp, message.source);
        strcat(sendMessTemp, ":");
        strcat(sendMessTemp, message.data);

        if ((numbytes = send(sockfd, (const void *)sendMessTemp, strlen(sendMessTemp), 0)) == -1) {
            perror("send");
            close(sockfd);
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    bool flag;
    char clientID[MAX_NAME] = "None~";
    char password[MAX_NAME];
    char serverIP[MAX_NAME];
    char portNum[MAX_NAME];
    struct Message message;

    if (argc != 1) {
        fprintf(stderr,"usage (write ftp as is): talker\n");
        exit(1);
    }

    while (true) {
        printf("Please Login or Register\n");
        printf("The login command is /login <username> <password> <server ip> <port number>\n");
        printf("To register type /register <username> <password> <server ip> <port number>\n");
        printf("To quit type /quit\n");

        while (true) {
            char input[2048];

            fgets(input, sizeof(input), stdin);
            int newline_position = strcspn(input, "\n");
            input[newline_position] = '\0';

            char command[30];

            int index_save;

            flag = false;
            for (int i = 0; i < 30; i++) {
                if (i == 0) {
                    if (input[i] == '\0') {
                        printf("No input given\n");
                        flag = true;
                        break;
                    }

                    if (input[i] != '/') {
                        index_save = i;
                        sprintf(command, "text");
                        break;
                    }
                } else {
                    if (input[i] != ' ' && input[i] != '\0') {
                        command[i - 1] = input[i];
                    } else {
                        index_save = i;
                        command[i - 1] = '\0';
                        break;
                    }
                }
            }
            if (flag) {
                continue;
            }

            if (strcmp(command, "login") == 0) {
                message.type = LOGIN;

                if (input[index_save] == '\0') {
                    printf("No login detail found\n");
                    continue;
                } else if (input[index_save + 1] == '\0') {
                    printf("No login detail found\n");
                    continue;
                }

                flag = false;
                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] == '\0') {
                        printf("Either not enough login info or no spaces\n");
                        flag = true;
                        break;
                    }

                    if (input[i] != ' ') {
                        clientID[i - index_save - 1] = input[i];
                    } else {
                        clientID[i - index_save - 1] = '\0';
                        index_save = i;
                        break;
                    }
                }
                if (flag) {
                    continue;
                }

                if (input[index_save + 1] == '\0') {
                    printf("Not enough login info\n");
                    continue;
                }

                flag = false;
                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] == '\0') {
                        printf("Either not enough login info or no spaces\n");
                        flag = true;
                        break;
                    }

                    if (input[i] != ' ') {
                        password[i - index_save - 1] = input[i];
                    } else {
                        password[i - index_save - 1] = '~';
                        password[i - index_save] = '\0';
                        index_save = i;
                        break;
                    }
                }
                if (flag) {
                    continue;
                }

                if (input[index_save + 1] == '\0') {
                    printf("Not enough login info\n");
                    continue;
                }

                flag = false;
                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] == '\0') {
                        printf("Either not enough login info or no spaces\n");
                        flag = true;
                        break;
                    }

                    if (input[i] != ' ') {
                        serverIP[i - index_save - 1] = input[i];
                    } else {
                        serverIP[i - index_save - 1] = '\0';
                        index_save = i;
                        break;
                    }
                }
                if (flag) {
                    continue;
                }

                if (input[index_save + 1] == '\0') {
                    printf("Not enough login info\n");
                    continue;
                }

                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] != '\0') {
                        portNum[i - index_save - 1] = input[i];
                    } else {
                        portNum[i - index_save - 1] = '\0';
                        break;
                    }
                }

                sprintf(message.source, clientID);
                sprintf(message.data, password);

                message.size = strlen(message.data) + 4 + strlen(message.source);
                int num_digits = calcdigits(message.size);
                num_digits = calcdigits(message.size + num_digits);
                message.size += num_digits;

                break;
            } else if (strcmp(command, "register") == 0) {
                message.type = REGISTER;

                if (input[index_save] == '\0') {
                    printf("No register detail found\n");
                    continue;
                } else if (input[index_save + 1] == '\0') {
                    printf("No register detail found\n");
                    continue;
                }

                flag = false;
                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] == '\0') {
                        printf("Either not enough registration info or no spaces\n");
                        flag = true;
                        break;
                    }

                    if (input[i] != ' ') {
                        clientID[i - index_save - 1] = input[i];
                    } else {
                        clientID[i - index_save - 1] = '\0';
                        index_save = i;
                        break;
                    }
                }
                if (flag) {
                    continue;
                }

                if (input[index_save + 1] == '\0') {
                    printf("Not enough registration info\n");
                    continue;
                }

                flag = false;
                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] == '\0') {
                        printf("Either not enough registration info or no spaces\n");
                        flag = true;
                        break;
                    }

                    if (input[i] != ' ') {
                        password[i - index_save - 1] = input[i];
                    } else {
                        password[i - index_save - 1] = '~';
                        password[i - index_save] = '\0';
                        index_save = i;
                        break;
                    }
                }
                if (flag) {
                    continue;
                }

                if (input[index_save + 1] == '\0') {
                    printf("Not enough registration info\n");
                    continue;
                }

                flag = false;
                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] == '\0') {
                        printf("Either not enough registration info or no spaces\n");
                        flag = true;
                        break;
                    }

                    if (input[i] != ' ') {
                        serverIP[i - index_save - 1] = input[i];
                    } else {
                        serverIP[i - index_save - 1] = '\0';
                        index_save = i;
                        break;
                    }
                }
                if (flag) {
                    continue;
                }

                if (input[index_save + 1] == '\0') {
                    printf("Not enough registration info\n");
                    continue;
                }

                for (int i = index_save + 1; i < 1050; i++) {
                    if (input[i] != '\0') {
                        portNum[i - index_save - 1] = input[i];
                    } else {
                        portNum[i - index_save - 1] = '\0';
                        break;
                    }
                }

                sprintf(message.source, clientID);
                sprintf(message.data, password);

                message.size = strlen(message.data) + 4 + strlen(message.source);
                int num_digits = calcdigits(message.size);
                num_digits = calcdigits(message.size + num_digits);
                message.size += num_digits;
                break;
            } else if (strcmp(command, "quit") == 0) {
                return 0;
            } else {
                printf("Need to login or register first\n");
            }
        }

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo((char *)serverIP, (char *)portNum, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and make a socket
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("talker: socket");
                continue;
            }

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("connect");
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "talker: failed to create socket\n");
            return 2;
        }

        char sendMess[MAX_NAME + MAX_DATA + 6 + calcdigits(MAX_DATA + MAX_NAME)];

        char str1[10];

        sprintf(sendMess, "%d", message.type);
        strcat(sendMess, ":");
        sprintf(str1, "%d", message.size);
        strcat(sendMess, str1);
        strcat(sendMess, ":");
        strcat(sendMess, message.source);
        strcat(sendMess, ":");
        strcat(sendMess, message.data);

        if ((numbytes = send(sockfd, (const void *)sendMess, strlen(sendMess), 0)) == -1) {
            perror("send");
            close(sockfd);
            exit(1);
        }

        char buf[10000];
        if ( (numbytes = recv(sockfd, buf, 10000, 0)) == -1)
        {
            perror("accept");
            exit(1);
        }

        if (numbytes == 0) {
            printf("Server Disconnected\n");
            freeaddrinfo(servinfo);
            close(sockfd);
            return 0;
        }

        if (buf[0] == '2') {
            printf("Login Authentication Failed\n");
            continue;
        }

        if (buf[0] == '1' && buf[1] == '9') {
            printf("Registration Failed - Username in use\n");
            continue;
        }

        printf("You can now join or create a session\n");
        printf("To create a session type /createsession <session id>\n");
        printf("To join a session type /joinsession <session id>\n");
        printf("To get the list of all current sessions with users type /list\n");
        printf("You may choose to privately text anyone with the syntax /priv <username>+<message>\n");
        printf("You can logout with the command /logout\n");

        while (true) {
            globalLogout = false;
            globalQuit = false;
            globalJoin = false;
            globalSess = false;

            pthread_t rec_thread_first;
            pthread_t state_thread;

            pthread_create(&rec_thread_first, NULL, receiveText, sockfd);
            pthread_detach(rec_thread_first);

            struct ThreadPass *info = (struct ThreadPass*)malloc(sizeof(struct ThreadPass));
            info->sockfd = sockfd;
            sprintf(info->clientID, clientID);
            sprintf(info->serverIP, serverIP);

            pthread_create(&state_thread, NULL, secondState, (void *)info);
            pthread_detach(state_thread);

            while (true) {
                if (globalJoin || globalLogout || globalQuit || globalSess) {
                    break;
                }
            }

            if (globalLogout) {
                freeaddrinfo(servinfo);
                close(sockfd);
                break;
            }

            if (globalQuit) {
                freeaddrinfo(servinfo);
                close(sockfd);
                return 0;
            }

            if (globalJoin) {
                continue;
            }

            //-----------------------------------------------------------------------------------------------------------------------------
            //Texting Starts
            //-----------------------------------------------------------------------------------------------------------------------------

            printf("You are now in a session\n");
            printf("To send a message to everyone in the session type <message>\n");
            printf("You are still get a list of all users and sessions and be able to privately message users\n");

            globalLeave = false;
            globalLogout = false;
            globalQuit = false;

            pthread_t rec_thread;
            pthread_t fget_thread;

            pthread_create(&rec_thread, NULL, receiveText, sockfd);
            pthread_detach(rec_thread);

            struct ThreadPass *param = (struct ThreadPass*)malloc(sizeof(struct ThreadPass));
            param->sockfd = sockfd;
            sprintf(param->clientID, clientID);
            sprintf(param->serverIP, serverIP);

            pthread_create(&fget_thread, NULL, lastState, (void *)param);
            pthread_detach(fget_thread);

            while (true) {
                if (globalLeave || globalLogout || globalQuit) {
                    break;
                }
            }

            if (globalLogout) {
                freeaddrinfo(servinfo);
                close(sockfd);
                break;
            }

            if (globalQuit) {
                freeaddrinfo(servinfo);
                close(sockfd);
                return 0;
            }
        }
    }

    freeaddrinfo(servinfo);

    close(sockfd);

    return 0;
}