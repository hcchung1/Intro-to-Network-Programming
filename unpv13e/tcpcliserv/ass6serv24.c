#include "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <ctype.h>  // for strcasecmp (某些系統需要)

#define MAX_NAME_LENGTH 50
#define MAX_ROOMS 5
#define MAX_VISITORS 7
#define MAX_OBSERVERS 3
#define MAX_CLIENTS 1024
#define MAX_CLIENTS_PER_ROOM (1 + MAX_VISITORS + MAX_OBSERVERS)

/* 狀態常數 */
#define STAGE_INPUT_NAME 0
#define STAGE_SELECT_ROOM 1
#define STAGE_SET_PLAYER_COUNT 2
#define STAGE_SET_ROUNDS 3
#define STAGE_ROOM_OPERATION 4
#define STAGE_READY 5
#define STAGE_RUNNING 6

/* 全域資料 (不再放 stage[MAX_CLIENTS]) */
char client_names[MAX_CLIENTS][MAX_NAME_LENGTH];
int client_rooms[MAX_CLIENTS]; // client_fd 對應到哪個房間

/* 在「房間 × 玩家索引」存每位玩家的 stage */
int client_stage[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];
int non_room_stage[MAX_CLIENTS]; // 用來暫存輸入名字或選房間的 stage

/* 房間相關資訊 */
int room_clients[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];
int room_client_count[MAX_ROOMS];  // 每個房間已連線的客戶端數
char room_client_names[MAX_ROOMS][MAX_CLIENTS_PER_ROOM][MAX_NAME_LENGTH];
int room_max_players[MAX_ROOMS];   // 房主設定的「幾位玩家」
int room_ready[MAX_ROOMS];         // 紀錄房間是否已準備
int room_host[MAX_ROOMS];          // 房主的 FD
int room_rounds[MAX_ROOMS];        // 房主設定的回合數

/* 方便我們做「準備」與「回合中狀態」的flag */
int ready_status[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];

/* 先保留你的卡牌與遊戲變數，詳細邏輯依你需要 */
int card[35] = { 
    2,4,5,6,7,8,9,10,10,11,11,13,14,15,17,-1,-1,-1,-2,-2,
    -2,-3,-3,-3,-4,-4,-4,-5,-5,-5,100,101,102,103,104
};
int treasure_value[5] = {5, 7, 8, 10, 12};

int enable = 0;

typedef struct {
    int current_round;       
    int total_rounds;        
    int current_step;        
    time_t step_start_time;  
    int reply_count;         
    int step_status;         
    int is_game_over;        

    char decisions[MAX_CLIENTS_PER_ROOM]; 
    int active_player_count; 
    int this_round[MAX_CLIENTS_PER_ROOM];
    int scores[MAX_CLIENTS_PER_ROOM];
    int tempcoin[MAX_CLIENTS_PER_ROOM];
    int tragedy[5];
    int leftcoin;
    int appear[5];
} RoomState;

RoomState room_state[MAX_ROOMS]; // 每個房間一個


/* 前置函式宣告 */
void handle_client_message(int client_fd, fd_set *all_fds, int *max_fd);
void handle_client_disconnect(int client_fd, fd_set *all_fds);
void proceed_next_step(int room_number);
void finalize_step_decisions(int room_number);
void start_game(int room_number);

/* 取得client_fd在room_number房間的index，找不到則回-1 */
int get_client_index_in_room(int room_number, int client_fd) {
    int cnt = room_client_count[room_number];
    for (int i = 0; i < cnt; i++) {
        if (room_clients[room_number][i] == client_fd) {
            return i;
        }
    }
    return -1;
}

/* 初始化房間資料 */
void initialize_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        room_client_count[i] = 0;
        room_max_players[i]  = 0;
        room_ready[i]        = 0;
        room_host[i]         = -1;
        room_rounds[i]       = 0;

        for (int j = 0; j < MAX_CLIENTS_PER_ROOM; j++) {
            room_clients[i][j]         = -1;
            room_client_names[i][j][0] = '\0';
            ready_status[i][j]        = 0;

            // 新增：將每一個房間裡的 client_stage 初始化為 STAGE_INPUT_NAME (或-1)
            client_stage[i][j] = -1; 
        }

        // 初始化RoomState
        room_state[i].current_round   = 0;
        room_state[i].total_rounds    = 0;
        room_state[i].current_step    = 0;
        room_state[i].step_start_time = 0;
        room_state[i].reply_count     = 0;
        room_state[i].step_status     = 0;
        room_state[i].is_game_over    = 0;
        room_state[i].active_player_count = 0;
        room_state[i].leftcoin        = 0;

        for (int k = 0; k < MAX_CLIENTS_PER_ROOM; k++) {
            room_state[i].decisions[k]  = '\0';
            room_state[i].this_round[k] = 0;
            room_state[i].scores[k]     = 0;
            room_state[i].tempcoin[k]   = 0;
        }
        for (int t = 0; t < 5; t++) {
            room_state[i].tragedy[t] = 0;
            room_state[i].appear[t]  = 0;
        }
    }

    // 初始化 client_rooms 與 client_names
    for (int c = 0; c < MAX_CLIENTS; c++) {
        client_rooms[c] = -1;
        client_names[c][0] = '\0';
    }
}

/* 廣播訊息到指定房間（exclude_fd可排除某個FD） */
void broadcast_message(int room_number, const char *message, int exclude_fd) {
    for (int i = 0; i < room_client_count[room_number]; i++) {
        int cfd = room_clients[room_number][i];
        if (cfd != exclude_fd && cfd >= 0) {
            if (write(cfd, message, strlen(message)) < 0) {
                perror("Write to client failed");
            }
        }
    }
}

/* 處理客戶端斷線 */
void handle_client_disconnect(int client_fd, fd_set *all_fds) {
    char sendline[MAXLINE];
    int room_number = client_rooms[client_fd];

    printf("Client disconnected: FD %d\n", client_fd);
    close(client_fd);
    FD_CLR(client_fd, all_fds);

    if (room_number != -1) {
        // 找到客戶端在房間中的索引
        int index = get_client_index_in_room(room_number, client_fd);
        if (index != -1) {
            // 廣播離開消息
            snprintf(sendline, sizeof(sendline), "%s has left the room.\n", 
                     room_client_names[room_number][index]);
            broadcast_message(room_number, sendline, -1);

            // 移除客戶端
            int last = room_client_count[room_number] - 1;
            room_clients[room_number][index] = room_clients[room_number][last];
            strcpy(room_client_names[room_number][index], 
                   room_client_names[room_number][last]);

            // 將對應 stage 也重置
            client_stage[room_number][index] = -1;

            room_clients[room_number][last] = -1;
            room_client_names[room_number][last][0] = '\0';
            client_stage[room_number][last] = -1;

            room_client_count[room_number]--;

            // 更新房主
            if (room_host[room_number] == client_fd) {
                if (room_client_count[room_number] > 0) {
                    room_host[room_number] = room_clients[room_number][0];
                    snprintf(sendline, sizeof(sendline), 
                             "%s is now the new host of room %d.\n", 
                             room_client_names[room_number][0], room_number + 1);
                    broadcast_message(room_number, sendline, -1);
                } else {
                    // 房間無人，重置房間設置
                    room_host[room_number]         = -1;
                    room_max_players[room_number]  = 0;
                    room_rounds[room_number]       = 0;
                    room_ready[room_number]        = 0;
                    for (int i = 0; i < MAX_CLIENTS_PER_ROOM; i++) {
                        room_clients[room_number][i]         = -1;
                        room_client_names[room_number][i][0] = '\0';
                        ready_status[room_number][i]         = 0;
                        client_stage[room_number][i]         = -1;
                    }
                    snprintf(sendline, sizeof(sendline), 
                             "Room %d has been cleared as all players have left.\n",
                             room_number + 1);
                    broadcast_message(room_number, sendline, -1);
                }
            }
        }
        client_rooms[client_fd] = -1;
    }

    // 也可在這裡把 client_names[client_fd][0] = '\0'; 以防下個連線重用
    client_names[client_fd][0] = '\0';
    non_room_stage[client_fd] = 0;   // 回到無狀態
}

/* 洗牌卡 */
void shuffle(int *array, int size) {
    srand(time(NULL));
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

/* end_current_round: 結束當前回合 */
void end_current_round(int room_number)
{
    RoomState *st = &room_state[room_number];
    char buf[64];
    snprintf(buf, sizeof(buf), "Round %d ended.\n", st->current_round);
    broadcast_message(room_number, buf, -1);

    st->current_round++;
    st->current_step = 0;
    st->leftcoin = 0;
    st->active_player_count = room_client_count[room_number];
    
    for (int i = 0; i < 5; i++) {
        st->tragedy[i] = 0;
        st->appear[i] = 0;
    }
    int cnt = room_client_count[room_number];
    for (int i = 0; i < cnt; i++) {
        st->decisions[i]   = '\0'; 
        st->tempcoin[i]    = 0;   
        st->this_round[i]  = 1;
    }

    if (st->current_round > st->total_rounds) {
        broadcast_message(room_number, "All rounds done! Game Over!\n", -1);
        int winner = -1;
        int highest = -1;
        for (int i = 0; i < cnt; i++) {
            snprintf(buf, sizeof(buf), "%s 's total scores: %d\n",
                     room_client_names[room_number][i], st->scores[i]);
            broadcast_message(room_number, buf, -1);
            if (st->scores[i] > highest) {
                highest = st->scores[i];
                winner = i;
            }
        }
        snprintf(buf, sizeof(buf), "Congradulation to %s! 1st Total got %d score!\n", 
                 room_client_names[room_number][winner], highest);
        st->is_game_over = 1;
        return;
    } else {
        proceed_next_step(room_number);
    }
}

/* proceed_next_step: 進行下一個 step */
void proceed_next_step(int room_number)
{
    RoomState *st = &room_state[room_number];

    // if (st->current_round > st->total_rounds) {
    //     broadcast_message(room_number, "All rounds completed. Game Over!\n", -1);
    //     st->is_game_over = 1;
    //     return;
    // }

    st->current_step++;

    char sendline[MAXLINE];

    if (st->current_step == 1) {
        snprintf(sendline, sizeof(sendline), "Round %d start from NOW!\n", st->current_round);
        broadcast_message(room_number, sendline, -1);
        shuffle(card, 35);
        for (int i = 0; i < 5; i++) {
            st->tragedy[i] = 0;
            st->appear[i] = 0;
        }
    }

    snprintf(sendline, sizeof(sendline), "Step:%d ", st->current_step);
    broadcast_message(room_number, sendline, -1);

    int draw = card[st->current_step - 1];
    if (draw > 0 && draw <= 20) {
        snprintf(sendline, sizeof(sendline), "----Discovered %d gems!----\n", draw);
        broadcast_message(room_number, sendline, -1);
        /* 分鑽石 */
        int cnt = room_client_count[room_number];
        int alive = st->active_player_count;
        for (int i = 0; i < cnt; i++) {
            if (st->this_round[i] == 1) {
                st->tempcoin[i] += (draw / alive);
            }
        }
        int remainder = draw % alive;
        st->leftcoin += remainder;
        snprintf(sendline, sizeof(sendline), "%d gem(s) left on the floor.\n", st->leftcoin);
        broadcast_message(room_number, sendline, -1);
    }
    else if (draw < 0) {
        if (draw == -1) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY1:SNAKE----\n");
            broadcast_message(room_number, sendline, -1);
            st->tragedy[0]++;
        } else if (draw == -2) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY2:ROCKS----\n");
            broadcast_message(room_number, sendline, -1);
            st->tragedy[1]++;
        } else if (draw == -3) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY3:FIRE----\n");
            broadcast_message(room_number, sendline, -1);
            st->tragedy[2]++;
        } else if (draw == -4) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY4:SPIDERS----\n");
            broadcast_message(room_number, sendline, -1);
            st->tragedy[3]++;
        } else if (draw == -5) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY5:ZOMBIES----\n");
            broadcast_message(room_number, sendline, -1);
            st->tragedy[4]++;
        }
    }
    else if (draw >= 100) {
        st->appear[draw - 100] = 1;
        snprintf(sendline, sizeof(sendline),
                 "WOW! It's a treasure!!! Value: {%d}.\nREMEMBER: ONLY ONE person can bring it back...\n",
                 treasure_value[draw - 100]);
        broadcast_message(room_number, sendline, -1);
    }

    int finish_now = 0;
    for (int i = 0; i < 5; i++) {
        if (st->tragedy[i] >= 2) {
            /* 代表大家都沒收穫 */
            int cnt = room_client_count[room_number];
            for (int j = 0; j < cnt; j++) {
                if (st->this_round[j] == 1) {
                    st->tempcoin[j] = 0;
                    snprintf(sendline, sizeof(sendline), "%s went home with nothing.\n",
                             room_client_names[room_number][j]);
                    broadcast_message(room_number, sendline, -1);
                }
            }
            finish_now = 1;
            end_current_round(room_number);
            break;
        }
    }

    if (!finish_now) {
        st->step_status = 1; // WAITING_FOR_DECISION
        st->reply_count = 0;

        int cnt = room_client_count[room_number];
        for (int i = 0; i < cnt; i++) {
            if (st->this_round[i] == 1) {
                st->decisions[i] = '\0';
                snprintf(sendline, sizeof(sendline), 
                        "NOW, it's time to make a decision...Do you want to go home or stay? (y/n)\n");
                write(room_clients[room_number][i], sendline, strlen(sendline));
            } else {
                snprintf(sendline, sizeof(sendline), "Other players are making choices...\n");
                write(room_clients[room_number][i], sendline, strlen(sendline));
            }
        }
        st->step_start_time = time(NULL);
    }
}

/* 結算玩家選擇 */
void finalize_step_decisions(int room_number)
{
    RoomState *st = &room_state[room_number];
    char sendline[128];
    int cnt = room_client_count[room_number];

    int go_back = 0;
    for (int i = 0; i < cnt; i++) {
        if (st->decisions[i] == 'Y') {
            go_back++;
        }
    }

    int single = (go_back == 1) ? 1 : 0;

    for (int i = 0; i < cnt; i++) {
        if (st->this_round[i] == 1) {
            if (st->decisions[i] == 'Y') {
                st->scores[i] += st->tempcoin[i] + (st->leftcoin / go_back);
                st->leftcoin = st->leftcoin % go_back;
                for (int j = 0; j < 5; j++) {
                    if (st->appear[j] && single == 1) {
                        st->scores[i] += treasure_value[j];
                        st->appear[j] = 0;
                    }
                }
                st->this_round[i] = 0;
                snprintf(sendline, sizeof(sendline), "%s went home with score: %d\n",
                        room_client_names[room_number][i],
                        st->scores[i]);
                broadcast_message(room_number, sendline, -1);
                st->active_player_count--;
            } 
            else if (st->decisions[i] == 'N') {
                snprintf(sendline, sizeof(sendline), "%s decided to stay.\n",
                        room_client_names[room_number][i]);
                broadcast_message(room_number, sendline, -1);
            }
        }
        
    }

    printf("Active players: %d\n", st->active_player_count);

    if (st->active_player_count == 0) {
        broadcast_message(room_number, "All players went home, end of this round.\n", -1);
        for (int i = 0; i < cnt; i++) {
            // 可以廣播每個人的總分
            //snprintf(sendline, sizeof(sendline), "%s 's total scores: %d\n",
            //         room_client_names[room_number][i], st->scores[i]);
            //broadcast_message(room_number, sendline, -1);
        }
        end_current_round(room_number);
    } else {
        proceed_next_step(room_number);
    }
}

/* start_game: 初始化遊戲，進入第一 round, step=0 */
void start_game(int room_number) {
    RoomState *st = &room_state[room_number];
    st->current_round = 1;
    st->total_rounds  = room_rounds[room_number]; // 例如3
    st->current_step  = 0;
    st->reply_count   = 0;
    st->step_status   = 0;
    st->is_game_over  = 0;
    st->leftcoin      = 0;

    int cnt = room_client_count[room_number];
    st->active_player_count = cnt;
    for (int i = 0; i < cnt; i++) {
        st->decisions[i]  = '\0';
        st->scores[i]     = 0; 
        st->tempcoin[i]   = 0;   
        st->this_round[i] = 1;
    }
    for (int i = 0; i < 5; i++) {
        st->tragedy[i] = 0;
        st->appear[i]  = 0;
    }

    proceed_next_step(room_number);
}

/* 處理客戶端訊息主函式 */
void handle_client_message(int client_fd, fd_set *all_fds, int *max_fd) 
{
    char sendline[MAXLINE], readline[MAXLINE];
    int bytes_read = read(client_fd, readline, sizeof(readline) - 1);
    if (bytes_read <= 0) {
        handle_client_disconnect(client_fd, all_fds);
        return;
    }

    readline[bytes_read] = '\0';
    printf("Received from client FD %d: %s\n", client_fd, readline);

    int room_number = client_rooms[client_fd];

    if (room_number == -1) {

        if (non_room_stage[client_fd] == 0 && client_names[client_fd][0] == '\0') {
            // STAGE_INPUT_NAME
            readline[strcspn(readline, "\n")] = '\0';
            strncpy(client_names[client_fd], readline, MAX_NAME_LENGTH - 1);
            client_names[client_fd][MAX_NAME_LENGTH - 1] = '\0';

            snprintf(sendline, sizeof(sendline),
                     "Welcome, %s! Enter room number (1-%d):\n", 
                     client_names[client_fd], MAX_ROOMS);
            write(client_fd, sendline, strlen(sendline));

            non_room_stage[client_fd] = STAGE_SELECT_ROOM;
            return;
        }
        else if (non_room_stage[client_fd] == STAGE_SELECT_ROOM) {
            int chosen_room = atoi(readline) - 1;
            if (chosen_room < 0 || chosen_room >= MAX_ROOMS) {
                snprintf(sendline, sizeof(sendline),
                         "Invalid room number. Please enter a number between 1 and %d:\n",
                         MAX_ROOMS);
                write(client_fd, sendline, strlen(sendline));
                return;
            }
            // 如果房間滿了, etc...
            if (room_client_count[chosen_room] >= MAX_CLIENTS_PER_ROOM) {
                snprintf(sendline, sizeof(sendline), 
                         "Room %d is full. Try another room.\n", chosen_room + 1);
                write(client_fd, sendline, strlen(sendline));
                return;
            }
            // 加入房間
            int index = room_client_count[chosen_room];
            room_clients[chosen_room][index] = client_fd;
            strncpy(room_client_names[chosen_room][index],
                    client_names[client_fd],
                    MAX_NAME_LENGTH-1);
            room_client_names[chosen_room][index][MAX_NAME_LENGTH-1] = '\0';
            room_client_count[chosen_room]++;

            client_rooms[client_fd] = chosen_room;
            room_number = chosen_room;

            // 這時才有 "房間+index" => 可以指定 stage
            client_stage[room_number][index] = STAGE_ROOM_OPERATION;

            snprintf(sendline, sizeof(sendline),
                     "You have joined room %d.\n", chosen_room+1);
            write(client_fd, sendline, strlen(sendline));

            // 若此人是房主
            if (index == 0) {
                room_host[chosen_room] = client_fd;
                snprintf(sendline, sizeof(sendline),
                         "You are the host of room %d. Set the number of players (5-8):\n",
                         chosen_room + 1);
                write(client_fd, sendline, strlen(sendline));
                client_stage[room_number][index] = STAGE_SET_PLAYER_COUNT;
            } 
            else {
                snprintf(sendline, sizeof(sendline),
                         "%s has joined as guest.\n", client_names[client_fd]);
                broadcast_message(chosen_room, sendline, client_fd);
            }

            non_room_stage[client_fd] = 0; // 不用了
            return;
        }
        else {
            // 其他情況 => 可能是亂輸入
            snprintf(sendline, sizeof(sendline), 
                     "Please input your name or select a room first.\n");
            write(client_fd, sendline, strlen(sendline));
            return;
        }
    }

    int idx = get_client_index_in_room(room_number, client_fd);
    if (idx < 0) {
        snprintf(sendline, sizeof(sendline), "Error: Not in the room.\n");
        write(client_fd, sendline, strlen(sendline));
        return;
    }

    int stg = client_stage[room_number][idx];
    printf("client_fd=%d => room=%d, index=%d, stage=%d\n", 
           client_fd, room_number, idx, stg);

    switch(stg) {
    case STAGE_SET_PLAYER_COUNT: {
        long value = atol(readline);
        if (value >= 5 && value <= 8) {
            room_max_players[room_number] = (int)value;
            snprintf(sendline, sizeof(sendline),
                     "Player numbers set to %d. Please enter the round numbers (3-5):\n",
                     room_max_players[room_number]);
            write(client_fd, sendline, strlen(sendline));
            client_stage[room_number][idx] = STAGE_SET_ROUNDS;
        } else {
            snprintf(sendline, sizeof(sendline), 
                     "Invalid input. Please enter a number between 5 and 8:\n");
            write(client_fd, sendline, strlen(sendline));
        }
        break;
    }
    case STAGE_SET_ROUNDS: {
        long value = atol(readline);
        if (value >= 3 && value <= 5) {
            room_rounds[room_number] = (int)value;
            room_ready[room_number]  = 1;

            snprintf(sendline, sizeof(sendline),
                     "Round numbers set to %ld. Room setup complete! Wait for the players joining\n",
                     value);
            write(client_fd, sendline, strlen(sendline));
            broadcast_message(room_number, sendline, client_fd);

            client_stage[room_number][idx] = STAGE_ROOM_OPERATION;
        } else {
            snprintf(sendline, sizeof(sendline),
                     "Invalid input. Please enter a number between 3 and 5:\n");
            write(client_fd, sendline, strlen(sendline));
        }
        break;
    }
    case STAGE_ROOM_OPERATION: {
        // 如果是房主 => 開始遊戲 (start_game)
        readline[strcspn(readline, "\n")] = '\0';
        if (client_fd == room_host[room_number]) {
            // 先把大家的 stage 調成 STAGE_RUNNING
            int cnt = room_client_count[room_number];
            for (int i = 0; i < cnt; i++) {
                client_stage[room_number][i] = STAGE_RUNNING;
            }
            snprintf(sendline, sizeof(sendline), 
                     "Game in room %d has started! Prepare to explore!\n",
                     room_number + 1);
            broadcast_message(room_number, sendline, -1);
            start_game(room_number);
        } else {
            // 其他玩家 => 就廣播聊天
            snprintf(sendline, sizeof(sendline), "%s: %s\n",
                     room_client_names[room_number][idx], readline);
            broadcast_message(room_number, sendline, client_fd);
        }
        break;
    }
    case STAGE_RUNNING: {
        // 這裡是遊戲進行 => 若 step_status==1 => y/n
        readline[strcspn(readline, "\n")] = '\0';
        RoomState *rst = &room_state[room_number];
        if (rst->step_status == 1 && !rst->is_game_over) {
            if (rst->decisions[idx] == '\0') {
                if (strcasecmp(readline, "y") == 0) {
                    rst->decisions[idx] = 'Y';
                    rst->reply_count++;
                } else if (strcasecmp(readline, "n") == 0) {
                    rst->decisions[idx] = 'N';
                    rst->reply_count++;
                } else {
                    write(client_fd, "Invalid input. Type 'y' or 'n'.\n", 32);
                    return;
                }
                // 若已收齊 => finalize
                if (rst->reply_count == rst->active_player_count) {
                    rst->step_status = 0; // reset
                    finalize_step_decisions(room_number);
                }
            } else {
                write(client_fd, "You have already decided.\n", 27);
            }
        } else {
            // 不在等 => 當聊天
            snprintf(sendline, sizeof(sendline),
                     "%s (RUNNING): %s\n",
                     room_client_names[room_number][idx], readline);
            broadcast_message(room_number, sendline, client_fd);
        }
        break;
    }
    default:
        snprintf(sendline, sizeof(sendline), "Unknown stage. Disconnecting.\n");
        write(client_fd, sendline, strlen(sendline));
        handle_client_disconnect(client_fd, all_fds);
        break;
    }
}

/* main */
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

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(SERV_PORT + 5); // 你有宣告SERV_PORT，所以+5

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

                    // 送初始訊息 (提示輸入名字)
                    const char *welcome_msg = "Enter your name:\n";
                    write(client_fd, welcome_msg, strlen(welcome_msg));

                    // 此時尚未決定房間, 不設定 stage[room][index], 
                    // 先等他輸完名字、選房 => handle_client_message() 
                    // 也可用一個 non_room_stage[] 暫存
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
