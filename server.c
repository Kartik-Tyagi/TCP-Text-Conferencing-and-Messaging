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
#include <stdbool.h>
#include <time.h>
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
#define NUM_USERS 1024
#define MAX_SESS 5

int user_count = 0;

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

struct user{
    char username[MAX_NAME];
    char password[MAX_DATA];
    char sess_name[MAX_DATA];
    int logged_in;
    int accept_fd;
};

struct session{
    char session_name[MAX_DATA];
    struct user users[NUM_USERS];
    // struct user *users;    
    int num_users;
};

struct user users[NUM_USERS];
struct session sessions[MAX_SESS];

int find_sess(char name[]) {

    for (int i = 0; i < MAX_SESS; i++)
    {
        if (sessions[i].num_users != 0)
        {
            if (strcmp(sessions[i].session_name, name) == 0)
            {
                return i;
            }
        }
    }
    
    return -1;
}

void add_sess(char name[], struct user j)
{
    for (int i = 0; i < MAX_SESS; i++)
    {
        if (sessions[i].num_users == 0)
        {
            strcpy(sessions[i].session_name, name);
            sessions[i].users[0] = j;
            sessions[i].num_users = 1;
            break;
        }        
    }
}


void add_user_to_sess(char sess_name[], struct user urmom)
{
    int i = find_sess(sess_name);
    sessions[i].users[sessions[i].num_users] = urmom;
    sessions[i].num_users++;

}



char * msg2str(int type, int size, char* src, char* data)
{
    char msg_str [10000] = "";
    char *msg_ptr = msg_str;
    char temp_1 [10000];
    char temp_2 [10000];

    sprintf(temp_1, "%d", type);
    sprintf(temp_2, "%d", size);

    strcat(msg_str, temp_1);
    strcat(msg_str, ":");
    strcat(msg_str, temp_2);
    strcat(msg_str, ":");
    strcat(msg_str, src);
    strcat(msg_str, ":");
    strcat(msg_str, data);
    strcat(msg_str, "~");
    return msg_ptr;
}

 


void client(int sockfd)
{

    FILE *user_file = fopen("users.txt", "a+");
    if (user_file == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_storage their_addr;
    char buf[10000];
    char buf2[1024];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    double droprate = 0.1;

    int accept_fd;
    addr_len = sizeof their_addr;
    if (( accept_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("accept");
        exit(1);
    }
    char* tok_1;
    char* tok_2;
    char* tok_3;

    char* temp1 = NULL;
    char* temp2 = NULL;
    char* temp3 = NULL;

    int count = 0;
    int numbytes = 1;
    int user_no = -1;
    char src[10000] = "";

    while (numbytes != 0)
    {
        if ( (numbytes = recv(accept_fd, buf, 1024, 0)) == -1)
        {
            perror("accept");
            exit(1);
        }
        if (numbytes == 0)
        {
            // printf("Ctrl - C occured\n");
            for (int i = 0; i < user_count; i++)
            {
                if (strcmp(users[i].username, src)  == 0)
                {
                    user_no = i;
                    break;
                }
            }
            if (user_no)
            {
                users[user_no].logged_in = 0;

                // Make sure user is removed from sessions
                    int a = find_sess(users[user_no].sess_name);
                    int b = -1;

                    for (int i = 0; i < user_count; i++)
                    {
                        if (strcmp(sessions[a].users[i].username, users[user_no].username) == 0)
                        {
                            b = i;
                            break;
                        }
                    }
                    if (b != -1)
                    {
                        for (int i = b; i < sessions[a].num_users-1; i++)
                        {
                            sessions[a].users[i] = sessions[a].users[i+1];
                        }
                        sessions[a].num_users--;
                    }                
            }
            break;
        }
        buf[numbytes] = '\0';
        printf("unfiltered msg %s\n", buf);

        tok_1 = strtok_r(buf, "~", &temp1);
        while (tok_1 != NULL)
        {
            printf("msg received: %s\n", tok_1);                        
            tok_2 = strtok_r(tok_1, ":", &temp2);
            count = 0;

            int msg_type; 
            int msg_size;
            char *msg_src;
            char *msg_data;

            int fields_ack = 0; 
            int user_found = 0;

            while (tok_2 != NULL)
            {
                if (count == 0)
                {
                    msg_type = atoi(tok_2);
                    // printf("msg type is %d\n", msg_type);
                }
                if (count == 1)
                {
                    msg_size = atoi(tok_2);
                    // printf("msg size is %s\n", tok_2);
                }
                if (count == 2)
                {
                    msg_src = tok_2;
                    // printf("msg src is %s\n", msg_src);
                }
                if (count == 3)
                {
                    msg_data = tok_2;
                    // printf("msg data is %s\n", msg_data);
                    count = -1;
                    fields_ack++;
                }
                
                ////////////

                //printf("%s\n", tok_2);
                tok_2 = strtok_r(NULL, ":", &temp2);
                count++;
            }

                if (fields_ack)
                {
                    for (int i = 0; i < user_count; i++)
                    {
                        // printf("Username is %s\n",users[i].username);
                        // printf("Password is %s\n",users[i].password);
                        // printf("\n");
                        // printf("Msg_src is %s\n",msg_src);
                        // printf("Msg_src is %s\n",msg_data);
                        // printf("\n");
                        strcpy(src,msg_src);

                        if ((strcmp(users[i].username, msg_src) == 0) && (strcmp(users[i].password, msg_data) == 0) && (users[i].logged_in == 0))
                        {
                            // printf("It entered the loop\n");
                            user_found++;
                            users[i].logged_in = 1;
                            users[i].accept_fd = accept_fd;
                            user_no = i;
                        }

                    }
                }
                /////////////
                if (msg_type == LOGIN && fields_ack)
                {
                    if (user_found) {
                        char * resp = msg2str(LO_ACK, 23, "server", "successful");
                        // printf("%s\n", resp);
                        fields_ack = 0;
                        user_found = 0;
                        
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        }
                    }
                    else
                    {
                        // printf("The comparison result for user:%d\n", strcmp(users[user_count-1].username, msg_src));
                        // printf("The comparison result for password:%d\n", strcmp(users[user_count-1].password, msg_data));
                        // printf("The password recieved:%s\n", msg_data);
                        // printf("DB password:%s\n", users[user_count-1].password);
                        // printf("The password recieved len:%d\n", strlen(msg_data));
                        // printf("DB password len:%d\n", strlen(users[user_count-1].password));
                        // printf("Last user is %s\n", users[user_count-1].username);
                        char * resp = msg2str(LO_NAK, 20, "server", "userDNE");
                        // printf("%s\n", resp);
                        fields_ack = 0;
                        user_found = 0;
                        
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        }
                    }

                }
                if (msg_type == NEW_SESS  && fields_ack)
                {
                    int a = find_sess(msg_data);
                    if (a==-1)
                    {
                        int uno; 
                        for (int i = 0; i < user_count; i++)
                        {
                            if (strcmp(users[i].username, msg_src) == 0)
                            {
                                uno = i;
                                break;
                            }
                        }
                        add_sess(msg_data, users[uno]);
                        strcpy(users[uno].sess_name , msg_data);                   
                        char * resp = msg2str(NS_ACK, 20, "server", "successful");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        }                               
                    }
                    else
                    {
                        char * resp = msg2str(NS_ACK, 20, "server", "fail");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        }                              
                    }
                }
                if (msg_type == JOIN && fields_ack)
                {
                    int a = find_sess(msg_data);
                    if (a == -1)
                    {
                        char * resp = msg2str(JN_NAK, 20, "server", "fail");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        }   
                    }
                    else
                    {
                        for (int i = 0; i < user_count;i++)
                        {
                            if (strcmp(users[i].username, msg_src) == 0)
                            {
                                add_user_to_sess(sessions[a].session_name, users[i]);
                                strcpy(users[i].sess_name , sessions[a].session_name);                   
                                break;
                            }
                        }
                        char * resp = msg2str(JN_ACK, 20, "server", "sucessful");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        }                           
                    }
                } 
                if (msg_type == MESSAGE && fields_ack)
                {
                    // Find session of msg_src
                    int a = -1;
                    for (int i =0; i < user_count; i++)
                    {
                        if (strcmp(users[i].username, msg_src) == 0)
                        {
                            a = find_sess(users[i].sess_name);
                            break;
                        }
                    }
                    if (a != -1)
                    {
                        for (int i = 0; i < sessions[a].num_users; i++)
                        {
                            if (strcmp(sessions[a].users[i].username, msg_src) != 0)
                            {
                                char temp[10000] = "";
                                strcat(temp, msg_src);
                                strcat(temp, " :");
                                strcat(temp, msg_data);
                                printf("Message being sent: %s\n", temp);
                                char * resp = msg2str(MESSAGE, 1000, "server", temp);
                                if ((numbytes = send(sessions[a].users[i].accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                                    perror("send");
                                    close(sockfd);
                                    exit(1);
                                }    
                                // printf("sent %d to %s\n", numbytes, sessions[a].users[i].username) ;                              
                            }
                        }
                    }

                }
                if (msg_type == LEAVE_SESS)
                {
                    // Find session of msg_src
                    int a = -1;
                    int b = -1;
                    for (int i =0; i < user_count; i++)
                    {
                        if (strcmp(users[i].username, msg_src) == 0)
                        {
                            a = find_sess(users[i].sess_name);
                            break;
                        }
                    }
                    for (int i = 0; i < user_count; i++)
                    {
                        if (strcmp(sessions[a].users[i].username, msg_src) == 0)
                        {
                            b = i;
                            break;
                        }
                    }
                    if (b != -1)
                    {
                        for (int i = b; i < sessions[a].num_users-1; i++)
                        {
                            sessions[a].users[i] = sessions[a].users[i+1];
                        }
                        sessions[a].num_users--;
                    }
                    // printf("Users in sessions after deletion:\n");
                    // for (int i = 0; i < sessions[a].num_users; i++)
                    // {
                    //     printf("User: %s\n", sessions[a].users[i].username);
                    // }
                    char * resp = msg2str(LEAVE_ACK, 20, "server", "yay");
                    if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                        perror("send");
                        close(sockfd);
                        exit(1);
                    }    
                }
                if (msg_type == EXIT)
                {
                    // Same stuff as leavesession 
                    int a = -1;
                    int b = -1;
                    for (int i =0; i < user_count; i++)
                    {
                        if (strcmp(users[i].username, msg_src) == 0)
                        {
                            a = find_sess(users[i].sess_name);
                            break;
                        }
                    }
                    for (int i = 0; i < user_count; i++)
                    {
                        if (strcmp(sessions[a].users[i].username, msg_src) == 0)
                        {
                            b = i;
                            break;
                        }
                    }
                    if (b != -1)
                    {
                        for (int i = b; i < sessions[a].num_users-1; i++)
                        {
                            sessions[a].users[i] = sessions[a].users[i+1];
                        }
                        sessions[a].num_users--;
                    }
                    // printf("Users in sessions after deletion:\n");
                    // for (int i = 0; i < sessions[a].num_users; i++)
                    // {
                    //     printf("User: %s\n", sessions[a].users[i].username);
                    // }
                    char * resp = msg2str(EXIT_ACK, 20, "server", "yay");
                    if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                        perror("send");
                        close(sockfd);
                        exit(1);
                    }    

                    // Making sure user can login again
                    for (int i = 0; i < user_count; i++)
                    {
                        if (strcmp(users[i].username, msg_src) == 0)
                        {
                            users[i].logged_in = 0;
                        }
                    }
                }
                if (msg_type == QUERY)
                {
                    char list[1024] = "";
                    int len = 0;
                    // printf("This is a test\n");
                    for (int i = 0; i < MAX_SESS; i++)
                    {
                        // printf("This is a test ==> 2\n");
                        if (sessions[i].num_users > 0)
                        {
                            // printf("Number of users => %d\n", sessions[i].num_users);
                            strcat(list, sessions[i].session_name);
                            len += strlen(sessions[i].session_name);
                            strcat(list,"-");
                            len += 1;
                            for (int j = 0; j < sessions[i].num_users; j++)
                            {
                                strcat(list, sessions[i].users[j].username);
                                len += strlen(sessions[i].users[j].username);
                                strcat(list, ",");
                                len +=1;
                            }
                            strcat(list,"|");
                            len+=1;

                        }
                    }
                    char * resp = msg2str(QU_ACK, 20, "server", list);

                    // char temp2[len];
                    // for (int i = 0; i<len; i++)
                    // {
                    //     temp2[i] = list[i];
                    // }
                    // char * resp = msg2str(QU_ACK, 20, "server", temp2);
                    // printf("%s\n",resp);

                    if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                        perror("send");
                        close(sockfd);
                        exit(1);
                    }    
                }
                if (msg_type == PRIV && fields_ack)
                {
                    char *privUser;
                    char *privMsg; 
                    tok_3 = strtok_r(msg_data, "+", &temp3);
                    privUser = tok_3;
                    tok_3 = strtok_r(NULL, ":", &temp3);
                    privMsg = tok_3;
                    char priv_msg_send[5000] = "";
                    strcat(priv_msg_send, msg_src);
                    strcat(priv_msg_send," : ");
                    strcat(priv_msg_send, privMsg);
                    int msg_sent  = 0;
                    for (int i = 0; i < user_count; i++)
                    {
                        if (strcmp(privUser, users[i].username) == 0)
                        {
                            printf("Private message string is %s\n", priv_msg_send);
                            char * resp = msg2str(PRIV, 20, "server", priv_msg_send);
                            if ((numbytes = send(users[i].accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                                perror("send");
                                close(sockfd);
                                exit(1);
                            }    
                            msg_sent = 1;
                        }
                    }
                    if (msg_sent)
                    {
                        char * resp = msg2str(PRIV_ACK, 20, "server", "yay");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        } 
                    }
                    else
                    {
                        char * resp = msg2str(PRIV_NACK, 20, "server", "nay");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        } 
                    }                       
                }
                if (msg_type == REGISTER && fields_ack)
                {
                    int check = 1;
                    for (int i = 0; i < user_count; i++)
                    {
                        if (strcmp(users[i].username, msg_src) == 0)
                        {
                            check = 0; 
                        }
                    }

                    if (check)
                    {
                        char temp[10000] = "";
                        strcat(temp, msg_src);
                        strcat(temp, " ");
                        strcat(temp, msg_data);
                        fprintf(user_file, "%s\n", temp);
                        struct user tempUser;
                        tempUser.logged_in = 1;
                        strcpy(tempUser.username, msg_src);
                        strcpy(tempUser.password, msg_data);

                        users[user_count] = tempUser;
                        user_count++;
                        fclose(user_file);
                        char * resp = msg2str(PRIV_ACK, 20, "server", "yay");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        } 

                        printf("User registered!\n");
                        printf("User count now is %d\n", user_count);
                    }
                    else
                    {
                        char * resp = msg2str(REG_NAK, 20, "server", "yay");
                        if ((numbytes = send(accept_fd, (const void *)resp, strlen(resp), 0)) == -1) {
                            perror("send");
                            close(sockfd);
                            exit(1);
                        }                         
                    }


                }

            tok_1 = strtok_r(NULL, "~", &temp1);
            // printf("\n");
        }
        // printf("\n\n");


        // printf("numbytes = %d\n", numbytes);
        buf[numbytes] = '\0';
        // printf(buf);
        // printf("\n");
        // break;
    } 
}


int main(int argc, char *argv[])
{
    // struct user user1;
    // strcpy(user1.username, "tyagika2");
    // strcpy(user1.password, "12345");
    // user1.logged_in = 0;

    // struct user user2;
    // strcpy(user2.username, "sinhata4");
    // strcpy(user2.password, "54321");
    // user2.logged_in = 0;

    // struct user user3;
    // strcpy(user3.username, "test123");
    // strcpy(user3.password, "qwert");
    // user3.logged_in = 0;

    char user_file_buf[10000];
    FILE *user_file = fopen("users.txt", "r");
    
    char *user_file_tok;
    char *user_file_temp;
    while (fgets(user_file_buf, 10000, user_file))
    {
        // printf("This is the user_file_buf: %s\n", user_file_buf);
        char *user;
        char *pass;
        user_file_tok = strtok_r(user_file_buf, " ", &user_file_temp);
        user = user_file_tok;
        user_file_tok = strtok_r(NULL, " \n", &user_file_temp);
        pass = user_file_tok;
        // printf("Username: %s\n", user);
        // printf("Password: %s\n", pass);
        struct user tempUser;
        tempUser.logged_in = 0;
        strcpy(tempUser.username, user);
        strcpy(tempUser.password, pass);

        users[user_count] = tempUser;
        user_count++;
    }
    fclose(user_file);
    // for (int i = 0; i < 3; i++)
    // {
    //     printf(users[i].username);
    //     printf("\n");
    //     printf(users[i].password);
    //     printf("\n");
    // }

    // users[0] = user1;
    // users[1] = user2;
    // users[2] = user3;

    for (int i = 0; i < MAX_SESS; i++)
    {
        sessions[i].num_users = 0;
    }

    // char users[NUM_USERS][1024] = {"tyagika2", "sinhata4", "test123"};

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[10000];
    char buf2[1024];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    double droprate = 0.1;

    srand(time(NULL));
    if (argc != 2)
    {
        printf("usage: server <port number>\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    if (listen(sockfd, 5) == -1) {
        fprintf(stderr, "listener: failed to listen socket\n");
        return 2;
    }    
    while(1)
    {
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, client, sockfd);
        pthread_detach(client_thread);
    }
}
