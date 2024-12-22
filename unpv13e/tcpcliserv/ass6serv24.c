#include "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 256
#define MAX_NAME_LENGTH 50
#define MAX_ROOMS 5
#define MAX_VISITORS 7
#define MAX_OBSERVERS 3
#define MAX_CLIENTS 1024
#define MAX_CLIENTS_PER_ROOM (1 + MAX_VISITORS + MAX_OBSERVERS)

int room_clients[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];
int room_client_count[MAX_ROOMS] = {0};
char room_client_names[MAX_ROOMS][MAX_CLIENTS_PER_ROOM][MAX_NAME_LENGTH];
int room_max_players[MAX_ROOMS] = {0};
int room_ready[MAX_ROOMS] = {0};
int room_host[MAX_ROOMS] = {-1};
int room_rounds[MAX_ROOMS] = {0};
int client_rooms[MAX_CLIENTS] = {-1};

void broadcast_message(int room_number, const char *message, int sender_fd) {
    for (int i = 0; i < room_client_count[room_number]; i++) {
        int client_fd = room_clients[room_number][i];
        if (client_fd != sender_fd) {
            write(client_fd, message, strlen(message));
        }
    }
}

// 處理遊戲邏輯
void start_game(int room_number) {
    char buffer[BUFFER_SIZE];
    int scores[MAX_CLIENTS_PER_ROOM] = {0};
    snprintf(buffer, sizeof(buffer), "Game in room %d has started! Prepare to explore!\n", room_number + 1);
    broadcast_message(room_number, buffer, -1);

    srand(time(NULL));
    int card[35] = {2,4,5,6,7,8,9,10,10,11,11,13,14,15,17,-1,-1,-1,-2,-2,-2,-3,-3,-3,-4,-4,-4,-5,-5,-5,0,0,0,0,0};
    int out_card[35] = {1};
    
    for (int round = 0; round < room_rounds[room_number]; round++) {
        int adv_status[MAX_CLIENTS_PER_ROOM] = {1};
        int remaining_players = room_max_players[room_number];
        int tempcoin[MAX_CLIENTS_PER_ROOM] = {0};
        int tragedy[5] = {0};
        int leftcoin = 0;
        clock_t start_time = clock();
        card[30+round] = 100 + round;
        int appear[5] = {0};
        int treasure_value[5] = {5, 7, 10, 12, 15};
        snprintf(buffer, sizeof(buffer), "Round %d start from NOW!\n", round + 1);
        broadcast_message(room_number, buffer, -1);
        int r = 0;
        int failed = 0;
        while ((clock() - start_time) < CLOCKS_PER_SEC) {
        // 持續等待，直到經過 1 秒
        }
        while (remaining_players != 0) {
            while ((clock() - start_time) < CLOCKS_PER_SEC) {
            // 持續等待，直到經過 1 秒
            }
            r++;
            snprintf(buffer, sizeof(buffer), "Step:%d  ", r);
            broadcast_message(room_number, buffer, -1);
            int y,z;
            do {
                y = rand() % 35;
                z = card[y];
            } while (z == 0 || out_card[y] == 0);
            out_card[y] = 0;

            if (z > 0 && z <= 20) {
                int t = z;
                snprintf(buffer, sizeof(buffer), "----Discovered %d gems!----\n", z);
                broadcast_message(room_number, buffer, -1);
                while ((t / remaining_players) > 0) {
                    for (int GEM = 0; GEM < room_max_players[room_number]; GEM++) {
                        if (adv_status[GEM] == 1) {
                            tempcoin[GEM]++;
                        }
                    }
                    t -= remaining_players;
                }
                leftcoin += t;
            } else if (z < 0) {
                if (z == -1) {
                    snprintf(buffer, sizeof(buffer), "----TRAGEDY1:SNAKE----\n");
                    broadcast_message(room_number, buffer, -1);
                    tragedy[0] += 1;
                } else if (z == -2) {
                    snprintf(buffer, sizeof(buffer), "----TRAGEDY2:ROCKS----\n");
                    broadcast_message(room_number, buffer, -1);
                    tragedy[1] += 1;
                } else if (z == -3) {
                    snprintf(buffer, sizeof(buffer), "----TRAGEDY3:FIRE----\n");
                    broadcast_message(room_number, buffer, -1);
                    tragedy[2] += 1;
                } else if (z == -4) {
                    snprintf(buffer, sizeof(buffer), "----TRAGEDY4:SPIDERS----\n");
                    broadcast_message(room_number, buffer, -1);
                    tragedy[3] += 1;
                } else if (z == -5) {
                    snprintf(buffer, sizeof(buffer), "----TRAGEDY5:ZOMBIES----\n");
                    broadcast_message(room_number, buffer, -1);
                    tragedy[4] += 1;
                }
                for (int T = 0; T < 5; T++) {
                    if (tragedy[T] == 2) {
                        failed = 1;
                    }
                }
            } else if (z >= 100) {
                appear[z - 100] = 1;
                snprintf(buffer, sizeof(buffer), "WOW! It's a treasure!!! Value: {%d}.\nREMEMBER: ONLY ONE person can bring it back...\n", treasure_value[z - 100]);
                broadcast_message(room_number, buffer, -1);
            }

            while ((clock() - start_time) < CLOCKS_PER_SEC) {
            // 持續等待，直到經過 1 秒
            }

            for (int i = 0; i < room_max_players[room_number]; i++) {
                if (adv_status[i] == 1) {
                    snprintf(buffer, sizeof(buffer), "%s have: %d gems\n", room_client_names[room_number][i], tempcoin[i]);
                    broadcast_message(room_number, buffer, -1);
                }
            }
            for (int i = 0; i < 5; i++) {
                if (appear[i] == 1) {
                    snprintf(buffer, sizeof(buffer), "There is a %d value treasure left.\n", treasure_value[i]);
                    broadcast_message(room_number, buffer, -1);
                }
            }
            snprintf(buffer, sizeof(buffer), "%d gem(s) left on the floor.\n", leftcoin);
            broadcast_message(room_number, buffer, -1);
            snprintf(buffer, sizeof(buffer), "NOW, it's time to make a decision...Do you want to go home or stay? (y/n)\n");
            broadcast_message(room_number, buffer, -1);
            

            // 設定回覆的超時時間
            const int TIMEOUT_SECONDS = 120;
            char player_choices[room_max_players[room_number]];

            for (int i = 0; i < room_max_players[room_number]; i++) {
                if (adv_status[i] == 1) {
                    snprintf(buffer, sizeof(buffer), "%s, do you want to go home or stay? (y/n):\n", room_client_names[room_number][i]);
                    write(room_clients[room_number][i], buffer, strlen(buffer));

                    // 開始計時
                    time_t start_time = time(NULL);
                    int bytes_read;
                    while (1) {
                        // 嘗試讀取回覆
                        bytes_read = read(room_clients[room_number][i], buffer, sizeof(buffer) - 1);
                        if (bytes_read > 0) {
                            buffer[bytes_read] = '\0'; // 確保字串以 NULL 結尾
                            player_choices[i] = buffer[0]; // 暫存玩家選擇
                            break;
                        }

                        // 檢查是否超時
                        if (difftime(time(NULL), start_time) > TIMEOUT_SECONDS) {
                            player_choices[i] = 'N'; // 超時視為回家
                            snprintf(buffer, sizeof(buffer), "%s did not respond in time and is assumed to have gone home.\n", room_client_names[room_number][i]);
                            broadcast_message(room_number, buffer, -1);
                            break;
                        }
                    }
                } else {
                    player_choices[i] = 'N'; // 已回家的玩家默認為 N
                }
            }

            // 公布所有玩家的選擇並更新狀態
            for (int i = 0; i < room_max_players[room_number]; i++) {
                if (adv_status[i] == 1) {
                    if (player_choices[i] == 'N' || player_choices[i] == 'n') {
                        adv_status[i] = 0; // 玩家選擇回家
                        snprintf(buffer, sizeof(buffer), "%s has chosen to go home.\n", room_client_names[room_number][i]);
                        broadcast_message(room_number, buffer, -1);
                    } else {
                        snprintf(buffer, sizeof(buffer), "%s has chosen to stay.\n", room_client_names[room_number][i]);
                        broadcast_message(room_number, buffer, -1);
                    }
                }
            }

            // 更新剩餘玩家數量
            remaining_players = 0;
            for (int i = 0; i < room_max_players[room_number]; i++) {
                if (adv_status[i] == 1) {
                    remaining_players++;
                }
            }

            // 如果沒有玩家剩下，結束當前回合
            if (remaining_players == 0) {
                snprintf(buffer, sizeof(buffer), "All players have gone home. Ending the round.\n");
                broadcast_message(room_number, buffer, -1);
                break;
            }

        }
    }
}

// 處理客戶端連線
void handle_client_message(int client_fd, fd_set *all_fds, int *max_fd) {
    static int stage[MAX_CLIENTS] = {0}; // 0: 輸入姓名, 1: 選擇房間, 2: 房間內操作
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        printf("Client disconnected: FD %d\n", client_fd);
        close(client_fd);
        FD_CLR(client_fd, all_fds);
        int room_number = client_rooms[client_fd];
        if (room_number != -1) {
            for (int i = 0; i < room_client_count[room_number]; i++) {
                if (room_clients[room_number][i] == client_fd) {
                    for (int j = i; j < room_client_count[room_number] - 1; j++) {
                        room_clients[room_number][j] = room_clients[room_number][j + 1];
                        strcpy(room_client_names[room_number][j], room_client_names[room_number][j + 1]);
                    }
                    room_client_count[room_number]--;
                    break;
                }
            }
        }
        return;
    }

    buffer[bytes_read] = '\0';
    printf("Received from client FD %d: %s\n", client_fd, buffer);

    int room_number = client_rooms[client_fd];

    if (stage[client_fd] == 0) {
        // 姓名輸入階段
        buffer[strcspn(buffer, "\n")] = '\0'; // 移除換行符
        snprintf(room_client_names[room_number][room_client_count[room_number]], MAX_NAME_LENGTH, "%s", buffer);
        snprintf(buffer, sizeof(buffer), "Welcome, %s! Enter room number (1-5):\n", room_client_names[room_number][room_client_count[room_number]]);
        write(client_fd, buffer, strlen(buffer));
        stage[client_fd] = 1;
    } else if (stage[client_fd] == 1) {
        // 選擇房間階段
        int chosen_room = atoi(buffer) - 1;
        if (chosen_room >= 0 && chosen_room < MAX_ROOMS) {
            if (room_ready[chosen_room] == 0 && room_client_count[chosen_room] > 0) {
                snprintf(buffer, sizeof(buffer), "Room %d is being set up. Try another room or wait.\n", chosen_room + 1);
                write(client_fd, buffer, strlen(buffer));
                return;
            }
            if (room_client_count[chosen_room] < MAX_CLIENTS_PER_ROOM) {
                client_rooms[client_fd] = chosen_room;

                if (room_client_count[chosen_room] == 0) {
                    // 房主
                    room_host[chosen_room] = client_fd;
                    snprintf(buffer, sizeof(buffer), "You are the host of room %d. Set the number of players (2-8) and rounds (1-5):\n", chosen_room + 1);
                } else {
                    int guest_number = room_client_count[chosen_room] + 1;
                    snprintf(buffer, sizeof(buffer), "You are guest no.%d in room %d.\n", guest_number, chosen_room + 1);
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "Waiting for the host to start the game.\n");

                    // 廣播新玩家加入
                    char join_message[BUFFER_SIZE];
                    snprintf(join_message, sizeof(join_message), "%s has joined room %d as guest no.%d.\n",
                             room_client_names[room_number][room_client_count[room_number]], chosen_room + 1, guest_number);
                    broadcast_message(chosen_room, join_message, client_fd);
                }

                room_clients[chosen_room][room_client_count[chosen_room]++] = client_fd;
                write(client_fd, buffer, strlen(buffer));
                stage[client_fd] = 2;
            } else {
                snprintf(buffer, sizeof(buffer), "Room %d is full. Try another room.\n", chosen_room + 1);
                write(client_fd, buffer, strlen(buffer));
            }
        } else {
            snprintf(buffer, sizeof(buffer), "Invalid room number. Try again.\n");
            write(client_fd, buffer, strlen(buffer));
        }
    } else if (stage[client_fd] == 2) {
        // 房間內操作階段
        if (client_fd == room_host[room_number]) {
            char *token = strtok(buffer, " ");
            if (strcmp(token, "players") == 0) {
                token = strtok(NULL, " ");
                int players = atoi(token);
                if (players >= 2 && players <= MAX_VISITORS + 1) {
                    room_max_players[room_number] = players;
                    snprintf(buffer, sizeof(buffer), "Players set to %d. Now set the rounds (1-5):\n", players);
                } else {
                    snprintf(buffer, sizeof(buffer), "Invalid number of players. Enter between 2 and 8.\n");
                }
            } else if (strcmp(token, "rounds") == 0) {
                token = strtok(NULL, " ");
                int rounds = atoi(token);
                if (rounds >= 1 && rounds <= 5) {
                    room_rounds[room_number] = rounds;
                    room_ready[room_number] = 1;
                    snprintf(buffer, sizeof(buffer), "Room setup complete! Waiting for players.\n");
                    broadcast_message(room_number, buffer, -1);
                } else {
                    snprintf(buffer, sizeof(buffer), "Invalid number of rounds. Enter between 1 and 5.\n");
                }
            } else {
                snprintf(buffer, sizeof(buffer), "Invalid command. Use 'players <num>' or 'rounds <num>'.\n");
            }
            write(client_fd, buffer, strlen(buffer));
        } else {
            snprintf(buffer, sizeof(buffer), "Message received: %s\n", buffer);
            write(client_fd, buffer, strlen(buffer));
        }
    }
}



int main() {
    int server_fd, client_fd, max_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    fd_set all_fds, read_fds;
    FD_ZERO(&all_fds);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERV_PORT + 5);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    FD_SET(server_fd, &all_fds);
    max_fd = server_fd;

    printf("Server listening on port %d...\n", SERV_PORT + 5);

    while (1) {
        read_fds = all_fds;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            break;
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd == server_fd) {
                    // 新客戶端連線
                    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fd < 0) {
                        perror("Accept failed");
                        continue;
                    }

                    printf("New client connected: FD %d\n", client_fd);
                    FD_SET(client_fd, &all_fds);
                    if (client_fd > max_fd) max_fd = client_fd;

                    // 發送初始訊息
                    const char *welcome_msg = "Enter room number (1-5):\n";
                    write(client_fd, welcome_msg, strlen(welcome_msg));
                } else {
                    // 處理現有客戶端訊息
                    handle_client_message(fd, &all_fds, &max_fd);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
