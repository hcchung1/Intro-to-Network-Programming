#include "unp.h"
#define MAX_CLIENTS 10
#define MAX_IDLEN 256 // ???check out this???
#define MAX_QUEUE 10 // 最大等待佇列長度

// void handle_chat(int client1, int client2, char* client1_id, char* client2_id);

int main(int argc, char **argv){

    // char recvline[MAXLINE], sendline[MAXLINE];

    // get the server file descriptor
    int server_fd = Socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // set socket options to avoid address already in use error
    int opt = 1;
    Setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    Setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERV_PORT+5); // assignment 6 port number
    Bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    Listen(server_fd, LISTENQ);

    // Signal(SIGCHLD, sig_chld);    /* must call waitpid() */

    struct sockaddr_in client_addr[10];
    int client_fds[10];

    fd_set rset;
    int num_clients = 0; // the number of client in the chat room now.
    char clients_id[MAX_CLIENTS][MAX_IDLEN]; // the id of each client
    int is_occupied[MAX_CLIENTS] = {0}; // 用於標記位置是否被佔用

    // 等待佇列的變數
    int waiting_queue[MAX_QUEUE];
    int front = 0, rear = 0, queue_size = 0;

    printf(" Server is running on port %d\n", SERV_PORT+5);

    while (1) {

        FD_ZERO(&rset);
        FD_SET(server_fd, &rset);
        int maxfd = server_fd;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if(is_occupied[i] == 0) continue;
            FD_SET(client_fds[i], &rset);
            if (client_fds[i] > maxfd) maxfd = client_fds[i];
        }

        // using select to find whether there is a new client want to connect
        Select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &rset)) {
            
            printf("New client is trying to connect...\n");
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_client_fd = Accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

            if (num_clients < MAX_CLIENTS) {
                int available_index = -1;
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (!is_occupied[i]) {
                        available_index = i;
                        break;
                    }
                }

                client_fds[available_index] = new_client_fd;
                is_occupied[available_index] = 1;
                num_clients++;

                char recvline[MAXLINE];
                int n = Read(new_client_fd, recvline, sizeof(recvline));
                recvline[n] = '\0';
                strcpy(clients_id[available_index], recvline);
                printf("New client (#%d) connected: %s\n", available_index + 1, clients_id[available_index]);

                // 通知其他用戶
                char msg[MAXLINE];
                char msggg[MAXLINE];
                sprintf(msg, "(#%d user %s enters)\n", available_index + 1, clients_id[available_index]);
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (is_occupied[i] && i != available_index) {
                        Write(client_fds[i], msg, strlen(msg));
                    }
                }

                // 發送歡迎訊息
                sprintf(msg, "You are the #%d user.\n", available_index + 1);
                Write(new_client_fd, msg, strlen(msg));
                sprintf(msggg, "You may now type in or wait for other users.\n");
                Write(new_client_fd, msggg, strlen(msggg));
            } 
            else {
                if (queue_size < MAX_QUEUE) {
                    // 加入等待佇列
                    waiting_queue[rear] = new_client_fd;
                    rear = (rear + 1) % MAX_QUEUE;
                    queue_size++;
                    printf("Chatroom full. Client added to waiting queue.\n");
                } else {
                    // 若等待佇列滿，拒絕新用戶
                    char *reject_msg = "Chatroom and waiting queue are full. Try again later.\n";
                    Write(new_client_fd, reject_msg, strlen(reject_msg));
                    Close(new_client_fd);
                }
            }
        }
        // Deal with chatting
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (FD_ISSET(client_fds[i], &rset)) {
                char buffer[MAXLINE];
                int n = Read(client_fds[i], buffer, sizeof(buffer));
                if (n == 0) {
                    if(num_clients == 2){
                        printf("Client %s disconnected\n", clients_id[i]);
                        char msgg[MAXLINE];
                        sprintf(msgg, "Bye!\n");
                        msgg[strlen(msgg)] = '\0';
                        Write(client_fds[i], msgg, strlen(msgg));
                        // Shutdown(client_fds[i], SHUT_WR); 
                        Close(client_fds[i]);
                        bzero(msgg, MAXLINE);
                        sprintf(msgg, "(%s left the room. You are the last one. Press Ctrl+D to leave or wait for a new user.)\n", clients_id[i]);
                        for (int j = 0; j < MAX_CLIENTS; ++j) {
                            if (is_occupied[j] && j != i) {
                                Write(client_fds[j], msgg, strlen(msgg));
                                break;
                            }
                        }
                    } 
                    else{
                        printf("Client %s disconnected\n", clients_id[i]); 
                        char message[MAXLINE];
                        sprintf(message, "Bye!\n");
                        message[strlen(message)] = '\0';
                        Write(client_fds[i], message, strlen(message));
                        // Shutdown(client_fds[i], SHUT_WR); 
                        Close(client_fds[i]);              
                        bzero(message, MAXLINE);
                        sprintf(message, "(%s left the room. %d users left)\n", clients_id[i], num_clients - 1);
                        message[strlen(message)] = '\0';
                        for (int j = 0; j < MAX_CLIENTS; ++j) {
                            if (is_occupied[j] && j != i) {
                                Write(client_fds[j], message, strlen(message));
                            }
                        }
                        bzero(message, MAXLINE);
                    }
                    
                    // bzero (clients_id[num_clients - 1], MAX_IDLEN);
                    // bzero (client_fds[(int)num_clients - 1], sizeof(int));
                    
                    is_occupied[i] = 0;
                    bzero (buffer, MAXLINE);
                    num_clients--;

                    printf("End of removing.\n");
                    
                    if (queue_size > 0) {
                        int available_index = -1;
                        for (int k = 0; k < MAX_CLIENTS; ++k) {
                            if (!is_occupied[k]) {
                                available_index = k;
                                break;
                            }
                        }

                        if (available_index != -1) {

                            int next_client_fd = waiting_queue[front];
                            front = (front + 1) % MAX_QUEUE;
                            queue_size--;

                            client_fds[available_index] = next_client_fd;
                            is_occupied[available_index] = 1;
                            num_clients++;

                            char recvline[MAXLINE];
                            int n = Read(next_client_fd, recvline, sizeof(recvline));
                            recvline[n] = '\0';
                            strcpy(clients_id[available_index], recvline);
                            printf("Client from waiting queue (#%d) connected: %s\n", available_index + 1, clients_id[available_index]);

                            char mmsg[MAXLINE];
                            sprintf(mmsg, "Welcome! You are the #%d user.\n", available_index + 1);
                            Write(next_client_fd, mmsg, strlen(mmsg));
                            sprintf(mmsg, "You may now type in or wait for other users.\n");
                            Write(next_client_fd, mmsg, strlen(mmsg));
                        }
                    }

                    printf("End of checking users waiting in queue.\n");

                } else if(n < 0){
                    perror("Read error");
                    // Shutdown(client_fds[i], SHUT_WR);
                    Close(client_fds[i]);
                }
                else{
                    // someone sends a message
                    buffer[n] = '\0';
                    char message[MAXLINE];
                    sprintf(message, "(%s) %s", clients_id[i], buffer);
                    message[strlen(message)] = '\0';
                    for (int j = 0; j < MAX_CLIENTS; ++j) {
                        if (is_occupied[j] && j != i) {
                            Write(client_fds[j], message, strlen(message));
                        }
                    }
                }
            }
        }
    }

    return 0;
}