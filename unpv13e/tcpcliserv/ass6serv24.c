#include  "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_ROOMS 5
#define MAX_CLIENTS_PER_ROOM 8
#define MAX_NAME_LENGTH 50

// 房間相關變數
int room_clients[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];
int room_client_count[MAX_ROOMS] = {0};
char room_client_names[MAX_ROOMS][MAX_CLIENTS_PER_ROOM][MAX_NAME_LENGTH];
pthread_mutex_t room_locks[MAX_ROOMS];
int room_max_players[MAX_ROOMS] = {0};
int room_rounds[MAX_ROOMS] = {0};
int room_ready[MAX_ROOMS] = {0};
int room_started[MAX_ROOMS] = {0};

// 廣播訊息
void broadcast_message(int room_number, const char *message, int sender_fd) {
    pthread_mutex_lock(&room_locks[room_number]);
    for (int i = 0; i < room_client_count[room_number]; i++) {
        int client_fd = room_clients[room_number][i];
        if (client_fd != sender_fd) {
            write(client_fd, message, strlen(message));
        }
    }
    pthread_mutex_unlock(&room_locks[room_number]);
}

// 初始化房間
void initialize_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        pthread_mutex_init(&room_locks[i], NULL);
        room_client_count[i] = 0;
        room_max_players[i] = 0;
        room_rounds[i] = 0;
        room_ready[i] = 0;
        room_started[i] = 0;
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
void *handle_client(void *client_socket) {
    int client_fd = *(int *)client_socket;
    free(client_socket);
    char buffer[BUFFER_SIZE];
    char client_name[MAX_NAME_LENGTH];
    int room_number = 0;

    // 要求客戶端輸入名稱
    write(client_fd, "Enter your name: \n", 18);
    int bytes_read = read(client_fd, client_name, sizeof(client_name) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return NULL;
    } else {
        printf("New client connected: %s\n", client_name);
    }
    client_name[bytes_read - 1] = '\0'; // 移除換行符

    while (1) {
        write(client_fd, "Enter room number (1-5):\n", 25);
        bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            close(client_fd);
            return NULL;
        } else printf("room number: %s\n", buffer);

        buffer[bytes_read] = '\0';
        room_number = atoi(buffer) - 1;

        if (room_number >= 0 && room_number < MAX_ROOMS) {
            printf("Client %s joined room %d\n", client_name, room_number + 1);
            break;
        } else {
            write(client_fd, "Invalid room number. Try again.\n", 32);
        }
    }

    pthread_mutex_lock(&room_locks[room_number]);
    if (room_client_count[room_number] < MAX_CLIENTS_PER_ROOM) {
        printf("Adding client %s to room %d\n", client_name, room_number + 1);
        room_clients[room_number][room_client_count[room_number]] = client_fd;
        strncpy(room_client_names[room_number][room_client_count[room_number]], client_name, MAX_NAME_LENGTH);
        room_client_count[room_number]++;

        // 廣播玩家加入的訊息
        snprintf(buffer, sizeof(buffer), "%s has joined the room! We now have %d player(s) in the room!\n",
                 client_name, room_client_count[room_number]);
        Write(client_fd, buffer, strlen(buffer));
        // broadcast_message(room_number, buffer, -1);

        if (room_client_count[room_number] == 1) {
            pthread_mutex_unlock(&room_locks[room_number]);
            snprintf(buffer, sizeof(buffer), "Set players (5-8):\n");
            write(client_fd, buffer, strlen(buffer));

            while (1) {
                bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) {
                    close(client_fd);
                    return NULL;
                } else printf("players: %s\n", buffer);
                buffer[bytes_read] = '\0';
                int players = atoi(buffer);
                if (players >= 5 && players <= 8) {
                    room_max_players[room_number] = players;
                    break;
                } else {
                    write(client_fd, "Invalid. Enter a number between 5 and 8:\n", 40);
                }
            }

            snprintf(buffer, sizeof(buffer), "Set rounds (3-5):\n");
            write(client_fd, buffer, strlen(buffer));

            while (1) {
                bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) {
                    close(client_fd);
                    return NULL;
                } else printf("rounds: %s\n", buffer);
                buffer[bytes_read] = '\0';
                int rounds = atoi(buffer);
                if (rounds >= 3 && rounds <= 5) {
                    room_rounds[room_number] = rounds;
                    room_ready[room_number] = 1;
                    break;
                } else {
                    write(client_fd, "Invalid. Enter a number between 3 and 5:\n", 40);
                }
            }

            snprintf(buffer, sizeof(buffer), "Room setup complete. Waiting for players.\n");
            write(client_fd, buffer, strlen(buffer));

            while (room_client_count[room_number] < room_max_players[room_number]) {
                // 等待其他玩家加入
            }

            snprintf(buffer, sizeof(buffer), "Game starting!\n");
            broadcast_message(room_number, buffer, -1);
            start_game(room_number);
        } else {
            pthread_mutex_unlock(&room_locks[room_number]);
            snprintf(buffer, sizeof(buffer), "Waiting for the game to start...\n");
            write(client_fd, buffer, strlen(buffer));
        }
    } else {
        pthread_mutex_unlock(&room_locks[room_number]);
        write(client_fd, "Room is full. Disconnecting.\n", 29);
        close(client_fd);
    }

    return NULL;
}

// 主程式
int main() {
    int server_fd, new_client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    initialize_rooms();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERV_PORT + 5);

    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    printf("Server listening...\n");

    while (1) {
        new_client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        printf("New client connected.\n");
        pthread_t client_thread;
        int *client_socket = malloc(sizeof(int));
        *client_socket = new_client_fd;
        pthread_create(&client_thread, NULL, handle_client, client_socket);
        pthread_detach(client_thread);
    }

    return 0;
}
