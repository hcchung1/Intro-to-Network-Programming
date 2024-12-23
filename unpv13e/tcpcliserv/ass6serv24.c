#include "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#define BUFFER_SIZE 256
#define MAX_NAME_LENGTH 50
#define MAX_ROOMS 5
#define MAX_VISITORS 7
#define MAX_OBSERVERS 3
#define MAX_CLIENTS 1024
#define MAX_CLIENTS_PER_ROOM (1 + MAX_VISITORS + MAX_OBSERVERS)

#define STAGE_INPUT_NAME 0
#define STAGE_SELECT_ROOM 1
#define STAGE_SET_PLAYER_COUNT 2
#define STAGE_SET_ROUNDS 3
#define STAGE_ROOM_OPERATION 4
#define STAGE_READY 5

// 客戶端名稱存儲
char client_names[MAX_CLIENTS][MAX_NAME_LENGTH];

// 房間相關資訊
int room_clients[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];
int room_client_count[MAX_ROOMS];
char room_client_names[MAX_ROOMS][MAX_CLIENTS_PER_ROOM][MAX_NAME_LENGTH];
int room_max_players[MAX_ROOMS];
int room_ready[MAX_ROOMS];
int room_host[MAX_ROOMS];
int room_rounds[MAX_ROOMS];
int client_rooms[MAX_CLIENTS];
int ready_status[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];

// 初始化房間設置
void initialize_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        room_client_count[i] = 0;
        room_max_players[i] = 0;
        room_ready[i] = 0;
        room_host[i] = -1;
        room_rounds[i] = 0;
        for (int j = 0; j < MAX_CLIENTS_PER_ROOM; j++) {
            room_clients[i][j] = -1;
            room_client_names[i][j][0] = '\0';
            ready_status[i][j] = 0;
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_rooms[i] = -1;
        client_names[i][0] = '\0';
    }
}

// 廣播訊息到指定房間
void broadcast_message(int room_number, const char *message, int exclude_fd) {
    for (int i = 0; i < room_client_count[room_number]; i++) {
        int client_fd = room_clients[room_number][i];
        if (client_fd != exclude_fd) {
            write(client_fd, message, strlen(message));
        }
    }
}

// 處理客戶端斷線
void handle_client_disconnect(int client_fd, fd_set *all_fds) {
    char buffer[BUFFER_SIZE];
    int room_number = client_rooms[client_fd];

    printf("Client disconnected: FD %d\n", client_fd);
    close(client_fd);
    FD_CLR(client_fd, all_fds);

    if (room_number != -1) {
        // 找到客戶端在房間中的索引
        int index = -1;
        for (int i = 0; i < room_client_count[room_number]; i++) {
            if (room_clients[room_number][i] == client_fd) {
                index = i;
                break;
            }
        }

        if (index != -1) {
            // 廣播離開消息
            snprintf(buffer, sizeof(buffer), "%s has left the room.\n", room_client_names[room_number][index]);
            broadcast_message(room_number, buffer, -1);

            // 移除客戶端
            for (int i = index; i < room_client_count[room_number] - 1; i++) {
                room_clients[room_number][i] = room_clients[room_number][i + 1];
                strcpy(room_client_names[room_number][i], room_client_names[room_number][i + 1]);
                ready_status[room_number][i] = ready_status[room_number][i + 1];
            }
            room_clients[room_number][room_client_count[room_number] - 1] = -1;
            room_client_names[room_number][room_client_count[room_number] - 1][0] = '\0';
            ready_status[room_number][room_client_count[room_number] - 1] = 0;
            room_client_count[room_number]--;

            // 更新房主
            if (room_host[room_number] == client_fd) {
                if (room_client_count[room_number] > 0) {
                    room_host[room_number] = room_clients[room_number][0];
                    snprintf(buffer, sizeof(buffer), "%s is now the new host of room %d.\n", room_client_names[room_number][0], room_number + 1);
                    broadcast_message(room_number, buffer, -1);
                } else {
                    // 房間無人，重置房間設置
                    room_host[room_number] = -1;
                    room_max_players[room_number] = 0;
                    room_rounds[room_number] = 0;
                    room_ready[room_number] = 0;
                    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
                        room_clients[room_number][i] = -1;
                        room_client_names[room_number][i][0] = '\0';
                        ready_status[room_number][i] = 0;
                    }
                    snprintf(buffer, sizeof(buffer), "Room %d has been cleared as all players have left.\n", room_number + 1);
                    broadcast_message(room_number, buffer, -1);
                }
            }
        }

        // 清空客戶端的房間資訊
        client_rooms[client_fd] = -1;
    }
}

// 處理客戶端訊息
void handle_client_message(int client_fd, fd_set *all_fds, int *max_fd) {
    static int stage[MAX_CLIENTS];
    // 初始化客戶端階段
    if (stage[client_fd] == 0 && client_names[client_fd][0] == '\0') {
        stage[client_fd] = STAGE_INPUT_NAME;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        handle_client_disconnect(client_fd, all_fds);
        return;
    }

    buffer[bytes_read] = '\0';
    printf("Received from client FD %d: %s\n", client_fd, buffer);

    int room_number = client_rooms[client_fd];

    switch (stage[client_fd]) {
        case STAGE_INPUT_NAME:
            // 姓名輸入階段
            buffer[strcspn(buffer, "\n")] = '\0'; // 移除換行符
            strncpy(client_names[client_fd], buffer, MAX_NAME_LENGTH - 1);
            client_names[client_fd][MAX_NAME_LENGTH - 1] = '\0';
            snprintf(buffer, sizeof(buffer), "Welcome, %s! Enter room number (1-%d):\n", client_names[client_fd], MAX_ROOMS);
            write(client_fd, buffer, strlen(buffer));
            stage[client_fd] = STAGE_SELECT_ROOM;
            break;

        case STAGE_SELECT_ROOM: {
            // 選擇房間階段
            int chosen_room = atoi(buffer) - 1;
            if (chosen_room < 0 || chosen_room >= MAX_ROOMS) {
                snprintf(buffer, sizeof(buffer), "Invalid room number. Please enter a number between 1 and %d:\n", MAX_ROOMS);
                write(client_fd, buffer, strlen(buffer));
                break;
            }

            if (room_ready[chosen_room] == 0 && room_client_count[chosen_room] > 0) {
                snprintf(buffer, sizeof(buffer), "Room %d is being set up. Try another room or wait.\n", chosen_room + 1);
                write(client_fd, buffer, strlen(buffer));
                break;
            }

            if (room_client_count[chosen_room] >= MAX_CLIENTS_PER_ROOM) {
                snprintf(buffer, sizeof(buffer), "Room %d is full. Try another room.\n", chosen_room + 1);
                write(client_fd, buffer, strlen(buffer));
                break;
            }

            // 加入房間
            room_clients[chosen_room][room_client_count[chosen_room]] = client_fd;
            strncpy(room_client_names[chosen_room][room_client_count[chosen_room]], client_names[client_fd], MAX_NAME_LENGTH - 1);
            room_client_names[chosen_room][room_client_count[chosen_room]][MAX_NAME_LENGTH - 1] = '\0';
            room_client_count[chosen_room]++;
            client_rooms[client_fd] = chosen_room;

            if (room_client_count[chosen_room] == 1) {
                // 第一位加入者成為房主
                room_host[chosen_room] = client_fd;
                snprintf(buffer, sizeof(buffer), "You are the host of room %d. Set the number of players (5-8):\n", chosen_room + 1);
                write(client_fd, buffer, strlen(buffer));
                stage[client_fd] = STAGE_SET_PLAYER_COUNT;
            } else {
                // 其他玩家成為客人
                int guest_number = room_client_count[chosen_room];
                snprintf(buffer, sizeof(buffer), "You are guest no.%d in room %d.\nWaiting for the host to set up the game.\n", guest_number, chosen_room + 1);
                write(client_fd, buffer, strlen(buffer));

                // 廣播新玩家加入
                snprintf(buffer, sizeof(buffer), "%s has joined room %d as guest no.%d.\n", client_names[client_fd], chosen_room + 1, guest_number);
                broadcast_message(chosen_room, buffer, client_fd);

                stage[client_fd] = STAGE_ROOM_OPERATION;
            }
            break;
        }

        case STAGE_SET_PLAYER_COUNT: {
            // 房主持有者設置玩家數
            long value = atol(buffer);
            if (value >= 5 && value <= 8) {
                room_max_players[room_number] = value;
                snprintf(buffer, sizeof(buffer), "Player numbers set to %ld. Please enter the round numbers (3-5):\n", value);
                write(client_fd, buffer, strlen(buffer));
                stage[client_fd] = STAGE_SET_ROUNDS;
            } else {
                snprintf(buffer, sizeof(buffer), "Invalid input. Please enter a number between 5 and 8:\n");
                write(client_fd, buffer, strlen(buffer));
            }
            break;
        }

        case STAGE_SET_ROUNDS: {
            // 房主持有者設置回合數
            long value = atol(buffer);
            if (value >= 3 && value <= 5) {
                room_rounds[room_number] = value;
                room_ready[room_number] = 1;
                snprintf(buffer, sizeof(buffer), "Round numbers set to %ld. Room setup complete! Waiting for players.\n", value);
                write(client_fd, buffer, strlen(buffer));
                broadcast_message(room_number, buffer, client_fd);
                stage[client_fd] = STAGE_ROOM_OPERATION;
            } else {
                snprintf(buffer, sizeof(buffer), "Invalid input. Please enter a number between 3 and 5:\n");
                write(client_fd, buffer, strlen(buffer));
            }
            break;
        }

        case STAGE_ROOM_OPERATION:
            // 房間內操作階段
            if (client_fd == room_host[room_number]) {
                // 房主等待開始遊戲的指令
                if (room_max_players[room_number] > 0 && room_rounds[room_number] > 0) {
                    snprintf(buffer, sizeof(buffer), "All players are ready. Type 'gogo' to start the game.\n");
                    write(client_fd, buffer, strlen(buffer));
                }
            } else {
                // 客人可以發送訊息或其他操作
                snprintf(buffer, sizeof(buffer), "Message received: %s\n", buffer);
                write(client_fd, buffer, strlen(buffer));
            }

            // 檢查是否達到人數上限
            if (room_client_count[room_number] == room_max_players[room_number]) {
                snprintf(buffer, sizeof(buffer), "Room %d is now full! All players, please type 'ok' or 'ready' to prepare.\n", room_number + 1);
                broadcast_message(room_number, buffer, -1);
                for (int i = 0; i < room_client_count[room_number]; i++) {
                    int guest_fd = room_clients[room_number][i];
                    stage[guest_fd] = STAGE_READY; // 切換到準備階段
                }
            }
            break;

        case STAGE_READY:
            // 準備階段
            if (strcasecmp(buffer, "ok") == 0 || strcasecmp(buffer, "ready") == 0) {
                // 找到客戶端在房間中的索引
                int index = -1;
                for (int i = 0; i < room_client_count[room_number]; i++) {
                    if (room_clients[room_number][i] == client_fd) {
                        index = i;
                        break;
                    }
                }

                if (index != -1) {
                    ready_status[room_number][index] = 1;
                    snprintf(buffer, sizeof(buffer), "%s is ready.\n", client_names[client_fd]);
                    broadcast_message(room_number, buffer, client_fd);

                    // 檢查所有玩家是否已準備
                    int all_ready = 1;
                    for (int i = 0; i < room_client_count[room_number]; i++) {
                        if (ready_status[room_number][i] == 0) {
                            all_ready = 0;
                            break;
                        }
                    }

                    if (all_ready) {
                        snprintf(buffer, sizeof(buffer), "All players are ready. Host, type 'gogo' to start the game.\n");
                        broadcast_message(room_number, buffer, -1);
                    }
                }
            } else if (client_fd == room_host[room_number] && strcasecmp(buffer, "gogo") == 0) {
                // 房主開始遊戲
                snprintf(buffer, sizeof(buffer), "Game starting now!\n");
                broadcast_message(room_number, buffer, -1);
                // 呼叫遊戲開始函數
                start_game(room_number);
                // 重置房間為未準備狀態
                room_ready[room_number] = 0;
                for (int i = 0; i < room_client_count[room_number]; i++) {
                    ready_status[room_number][i] = 0;
                    stage[room_clients[room_number][i]] = STAGE_ROOM_OPERATION;
                }
            } else {
                snprintf(buffer, sizeof(buffer), "Invalid command. Type 'ok' or 'ready'.\n");
                write(client_fd, buffer, strlen(buffer));
            }
            break;

        default:
            // 未定義的階段
            snprintf(buffer, sizeof(buffer), "Unknown stage. Disconnecting.\n");
            write(client_fd, buffer, strlen(buffer));
            handle_client_disconnect(client_fd, all_fds);
            break;
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
    int out_card[35] = {0};

    for (int round = 0; round < room_rounds[room_number]; round++) {
        int adv_status[MAX_CLIENTS_PER_ROOM] = {1};
        int remaining_players = room_max_players[room_number];
        int tempcoin[MAX_CLIENTS_PER_ROOM] = {0};
        int tragedy[5] = {0};
        int leftcoin = 0;
        time_t start_time = time(NULL);
        card[30 + round] = 100 + round;
        int appear[5] = {0};
        int treasure_value[5] = {5, 7, 10, 12, 15};
        snprintf(buffer, sizeof(buffer), "Round %d start from NOW!\n", round + 1);
        broadcast_message(room_number, buffer, -1);
        int r = 0;
        int failed = 0;

        // 等待 1 秒
        sleep(1);

        while (remaining_players != 0) {
            sleep(1);
            r++;
            snprintf(buffer, sizeof(buffer), "Step:%d  ", r);
            broadcast_message(room_number, buffer, -1);
            int y, z;
            do {
                y = rand() % 35;
                z = card[y];
            } while (z == 0 || out_card[y] == 1);
            out_card[y] = 1;

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

            sleep(1);

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
            char player_choices[MAX_CLIENTS_PER_ROOM];
            memset(player_choices, 'N', sizeof(player_choices)); // 默認為 'N'

            for (int i = 0; i < room_max_players[room_number]; i++) {
                if (adv_status[i] == 1) {
                    snprintf(buffer, sizeof(buffer), "%s, do you want to go home or stay? (y/n):\n", room_client_names[room_number][i]);
                    write(room_clients[room_number][i], buffer, strlen(buffer));

                    // 使用 select 設置超時
                    fd_set read_fds;
                    struct timeval timeout;
                    FD_ZERO(&read_fds);
                    FD_SET(room_clients[room_number][i], &read_fds);
                    timeout.tv_sec = TIMEOUT_SECONDS;
                    timeout.tv_usec = 0;

                    int ret = select(room_clients[room_number][i] + 1, &read_fds, NULL, NULL, &timeout);
                    if (ret > 0 && FD_ISSET(room_clients[room_number][i], &read_fds)) {
                        int choice = read(room_clients[room_number][i], buffer, sizeof(buffer) - 1);
                        if (choice > 0) {
                            buffer[choice] = '\0';
                            if (buffer[0] == 'y' || buffer[0] == 'Y') {
                                player_choices[i] = 'Y';
                            } else {
                                player_choices[i] = 'N';
                            }
                        }
                    } else {
                        // 超時視為回家
                        player_choices[i] = 'N';
                        snprintf(buffer, sizeof(buffer), "%s did not respond in time and is assumed to have gone home.\n", room_client_names[room_number][i]);
                        broadcast_message(room_number, buffer, -1);
                    }
                }
            }

            // 公布所有玩家的選擇並更新狀態
            for (int i = 0; i < room_max_players[room_number]; i++) {
                if (adv_status[i] == 1) {
                    if (player_choices[i] == 'N') {
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

            if (failed) {
                snprintf(buffer, sizeof(buffer), "Tragedy occurred! Game over for this round.\n");
                broadcast_message(room_number, buffer, -1);
                break;
            }
        }

        snprintf(buffer, sizeof(buffer), "Round %d has ended.\n", round + 1);
        broadcast_message(room_number, buffer, -1);
    }

    snprintf(buffer, sizeof(buffer), "Game in room %d has ended.\n", room_number + 1);
    broadcast_message(room_number, buffer, -1);
}

int main() {
    int server_fd, client_fd, max_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    fd_set all_fds, read_fds;
    FD_ZERO(&all_fds);

    initialize_rooms();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 設置 socket 選項以重用地址和端口
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
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
                    const char *welcome_msg = "Enter your name:\n";
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
