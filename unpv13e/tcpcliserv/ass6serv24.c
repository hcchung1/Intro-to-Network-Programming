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

/* 全域資料 */
char client_names[MAX_CLIENTS][MAX_NAME_LENGTH];
int client_rooms[MAX_CLIENTS]; // client_fd 對應到哪個房間

/* 房間相關資訊 */
int room_clients[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];
int room_client_count[MAX_ROOMS];  // 每個房間已連線的客戶端數
char room_client_names[MAX_ROOMS][MAX_CLIENTS_PER_ROOM][MAX_NAME_LENGTH];
int room_max_players[MAX_ROOMS];   // 房主設定的「幾位玩家」
int room_ready[MAX_ROOMS];        // 紀錄房間是否已準備
int room_host[MAX_ROOMS];         // 房主的 FD
int room_rounds[MAX_ROOMS];       // 房主設定的回合數

/* 方便我們做「準備」與「回合中狀態」的flag */
int ready_status[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];

/* 先保留你的卡牌與遊戲變數，詳細邏輯依你需要 */
int card[35] = { 
    2,4,5,6,7,8,9,10,10,11,11,13,14,15,17,-1,-1,-1,-2,-2,
    -2,-3,-3,-3,-4,-4,-4,-5,-5,-5,100,101,102,103,104
};

/* 以下只是先保留，不做詳細實作 */
int now_round[MAX_ROOMS];
int adv_status[MAX_ROOMS][MAX_CLIENTS_PER_ROOM];
int remaining_players[MAX_ROOMS];
int tempcoin[MAX_ROOMS][MAX_CLIENTS_PER_ROOM] = {0};
int tragedy[MAX_ROOMS][5] = {0};
int leftcoin[MAX_ROOMS] = {0};

int treasure_value[5] = {5, 7, 8, 10, 12};

int enable = 0;

typedef struct {
    int current_round;       // 現在第幾回合 (1-based)
    int total_rounds;        // 總回合數(房主設定)

    int current_step;        // 這個 round 裡的第幾個 step (1-based) 
                             // step 的數量可能隨機或視事件而定

    time_t step_start_time;  // 進入「等待玩家回覆」的開始時間，用來判斷超時
    int reply_count;         // 已收到多少玩家的回覆
    int step_status;         // 表示這個 step 處於哪個狀態(如: WAITING_FOR_EVENT, WAITING_FOR_DECISION, PROCESSING 等)

    int is_game_over;        // 若遊戲結束就標記 1

    /* 例如: 紀錄玩家當前回家/繼續的選擇 */
    char decisions[MAX_CLIENTS_PER_ROOM]; // 'Y'/'N' 或 '\0'
    int  active_player_count; // 還留在探險的玩家人數

    /* 你想追蹤的其他狀態，例如: 分數、寶物數量、等等... */
    int scores[MAX_CLIENTS_PER_ROOM];
    int tempcoin[MAX_CLIENTS_PER_ROOM];
    int tragedy[5];
    int leftcoin;
    int appear[5];
    // ...
} RoomState;

RoomState room_state[MAX_ROOMS]; // 每個房間一個

/* 函式宣告 */
void handle_client_message(int client_fd, fd_set *all_fds, int *max_fd);
void handle_client_disconnect(int client_fd, fd_set *all_fds);

/* 初始化房間資料 */
void initialize_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        room_client_count[i] = 0;
        room_max_players[i]  = 0;
        room_ready[i]        = 0;
        room_host[i]         = -1;
        room_rounds[i]       = 0;
        now_round[i]         = 0;
        for (int j = 0; j < MAX_CLIENTS_PER_ROOM; j++) {
            room_clients[i][j]                = -1;
            room_client_names[i][j][0]        = '\0';
            ready_status[i][j]               = 0;
            adv_status[i][j]                 = 1;
        }
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_rooms[i] = -1;
        client_names[i][0] = '\0';
    }
}

/* 廣播訊息到指定房間（exclude_fd可排除某個FD） */
void broadcast_message(int room_number, const char *message, int exclude_fd) {
    for (int i = 0; i < room_client_count[room_number]; i++) {
        int client_fd = room_clients[room_number][i];
        if (client_fd != exclude_fd) {
            if (write(client_fd, message, strlen(message)) < 0) {
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
        int index = -1;
        for (int i = 0; i < room_client_count[room_number]; i++) {
            if (room_clients[room_number][i] == client_fd) {
                index = i;
                break;
            }
        }

        if (index != -1) {
            // 廣播離開消息
            snprintf(sendline, sizeof(sendline), "%s has left the room.\n", 
                     room_client_names[room_number][index]);
            broadcast_message(room_number, sendline, -1);

            // 移除客戶端
            for (int i = index; i < room_client_count[room_number] - 1; i++) {
                room_clients[room_number][i] = room_clients[room_number][i + 1];
                strcpy(room_client_names[room_number][i], 
                       room_client_names[room_number][i + 1]);
                ready_status[room_number][i] = ready_status[room_number][i + 1];
            }
            // 清空最後一位
            room_clients[room_number][room_client_count[room_number] - 1] = -1;
            room_client_names[room_number]
                [room_client_count[room_number] - 1][0] = '\0';
            ready_status[room_number][room_client_count[room_number] - 1] = 0;
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
                    }
                    snprintf(sendline, sizeof(sendline), 
                             "Room %d has been cleared as all players have left.\n",
                             room_number + 1);
                    broadcast_message(room_number, sendline, -1);
                }
            }
        }

        // 清空客戶端的房間資訊
        client_rooms[client_fd] = -1;
    }
}

/* 
 * 如果你想在非阻塞框架中做「多回合、多 step」的遊戲流程，
 * 建議拆分成狀態機或用 thread；此處暫時保留 start_game() 結構供你參考。
 */
void shuffle(int *array, int size) {
    srand(time(NULL));
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void proceed_next_step(int room_number);


void end_current_round(int room_number)
{
    RoomState *st = &room_state[room_number];

    char buf[64];
    snprintf(buf, sizeof(buf), "Round %d ended.\n", st->current_round);
    broadcast_message(room_number, buf, -1);

    st->current_round++;
    st->current_step = 0;
    st->leftcoin = 0;
    
    for (int i = 0; i < 5; i++) {
        st->tragedy[i] = 0;
        st->appear[i] = 0;
    }

    for (int i = 0; i < room_client_count[room_number]; i++) {
        st->decisions[i] = '\0'; // 尚未決定
        st->tempcoin[i] = 0;   
    }
    

    if (st->current_round > st->total_rounds) {
        broadcast_message(room_number, "All rounds done! Game Over.\n", -1);
        st->is_game_over = 1;
        // 你可以在這裡把 stage[fd] = STAGE_ROOM_OPERATION; 或等玩家退出
    } else {
        // 進入下一回合
        proceed_next_step(room_number);
    }
}


void proceed_next_step(int room_number)
{
    // 先看看有沒有達到回合終點
    if (room_state[room_number].current_round > room_state[room_number].total_rounds) {
        // 全部回合都結束
        broadcast_message(room_number, "All rounds completed. Game Over!\n", -1);
        room_state[room_number].is_game_over = 1;
        return;
    }

    // 如果上一個 step 結束，需要進行下一個 step
    room_state[room_number].current_step++;

    char sendline[MAXLINE];
    // 如果是「剛開始這個 round 的第一個 step」，也可以做一次洗牌
    if (room_state[room_number].current_step == 1) {
        snprintf(sendline, sizeof(sendline), "Round %d start from NOW!\n", room_state[room_number].current_round);
        broadcast_message(room_number, sendline, -1);
        shuffle(card, 35);
        for (int i = 0; i < 5; i++) {
            room_state[room_number].tragedy[i] = 0;
            room_state[room_number].appear[i] = 0;
        }
    }

    // 廣播：進入這個 step
    snprintf(sendline, sizeof(sendline), "Step:%d ",room_state[room_number].current_step);
    // snprintf(sendline, sizeof(sendline), 
    //          "Round %d, Step %d\n",
    //          room_state[room_number].current_round,
    //          room_state[room_number].current_step);
    // broadcast_message(room_number, sendline, -1);

    // (1) 隨機產出事件並廣播
    // (2) 判斷事件後，更新狀態 (例如: 有人掛掉？分錢？)
    //     可能會做一些廣播
    int draw = card[room_state[room_number].current_step - 1];
    if (draw > 0 && draw <= 20) {
        // gems
        snprintf(sendline, sizeof(sendline), "----Discovered %d gems!----\n", draw);
        broadcast_message(room_number, sendline, -1);
        while ((draw / room_client_count[room_number])> 0) {
            for (int i = 0; i < room_client_count[room_number]; i++) {
                room_state[room_number].tempcoin[i] += 1;
            }
        }
        room_state[room_number].leftcoin += draw;
        snprintf(sendline, sizeof(sendline), "%d gem(s) left on the floor.\n", room_state[room_number].leftcoin);
    } else if (draw < 0) {
        // 災難
        if (draw == -1) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY1:SNAKE----\n");
            room_state[room_number].tragedy[0] += 1;
        } else if (draw == -2) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY2:ROCKS----\n");
            room_state[room_number].tragedy[1] += 1;
        } else if (draw == -3) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY3:FIRE----\n");
            room_state[room_number].tragedy[2] += 1;
        } else if (draw == -4) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY4:SPIDERS----\n");
            room_state[room_number].tragedy[3] += 1;
        } else if (draw == -5) {
            snprintf(sendline, sizeof(sendline), "----TRAGEDY5:ZOMBIES----\n");
            room_state[room_number].tragedy[4] += 1;
        }

        for (int i = 0; i < 5; i++) {
            if (room_state[room_number].tragedy[i] >= 2) {
                for (int j = 0; j < room_client_count[room_number]; j++) {
                    if (room_state[room_number]. decisions[j] == 'N' || room_state[room_number].decisions[j] == '\0') {
                    room_state[room_number].tempcoin[j] = 0;
                    snprintf(sendline, sizeof(sendline), "%s went home wuth nothing.\n", room_client_names[room_number][j]);
                    }
                }
                end_current_round(room_number);
            }
        }
    } else if (draw >= 100) {
        // 神器 appear
        room_state[room_number].appear[draw - 100] = 1;
        snprintf(sendline, sizeof(sendline), "WOW! It's a treasure!!! Value: {%d}.\nREMEMBER: ONLY ONE person can bring it back...\n", treasure_value[draw - 100]);
    }
    broadcast_message(room_number, sendline, -1);


    // (3) 檢查是否立即結束(例如：所有人都掛了)


    // (4) 若尚未結束 -> 讓玩家回覆是否要回家
    room_state[room_number].step_status = 1; // e.g. WAITING_FOR_DECISION
    room_state[room_number].reply_count = 0;
    for (int i = 0; i < room_client_count[room_number]; i++) {
        room_state[room_number].decisions[i] = '\0'; 
    }
    // snprintf(sendline, sizeof(sendline), "%d gem(s) left on the floor.\n", leftcoin);
    //         broadcast_message(room_number, sendline, -1);
    // 廣播：「請在 60s 內輸入 y/n：要不要回家」
    snprintf(sendline, sizeof(sendline), 
             "NOW, it's time to make a decision...Do you want to go home or stay? (y/n)\n");
    broadcast_message(room_number, sendline, -1);

    // 記錄開始等待的時間
    room_state[room_number].step_start_time = time(NULL);
}

void finalize_step_decisions(int room_number)
{
    RoomState *st = &room_state[room_number];
    char sendline[128];
    // (1) 根據每個玩家 'Y' or 'N' 來決定去留
    //     'Y' -> 回家: 算分, active_player_count--
    //     'N' -> 繼續探險
    for (int i = 0; i < room_client_count[room_number]; i++) {
        if (st->decisions[i] == 'Y') {
            // 幫他結算分數
            // st->scores[i] += ...
            // 廣播
            st->scores[i] += st->tempcoin[i];
            
            snprintf(sendline, sizeof(sendline), "%s went home with score: %d\n",
                     room_client_names[room_number][i],
                     st->scores[i]);
            broadcast_message(room_number, sendline, -1);
            // active_player_count--
            st->active_player_count--;
        } 
        else if (st->decisions[i] == 'N') {
            // 留下繼續
            // ...
            snprintf(sendline, sizeof(sendline), "%s decided to stay.\n",
                     room_client_names[room_number][i]);
            broadcast_message(room_number, sendline, -1);
        }
    }

    // (2) 廣播總結
    // broadcast_message(room_number, "Step decisions processed.\n", -1);

    // (3) 如果沒人留下( active_player_count == 0 )，或事件造成回合結束 => 結束本回合
    //     否則 => 進入下一個step
    if (st->active_player_count <= 0) {
        broadcast_message(room_number, "All players went home, end of this round.\n", -1);
        end_current_round(room_number); // 進入下個 round
    } else {
        // 進入下一個 step
        proceed_next_step(room_number);
    }
}

/* 範例的 start_game - 建議拆分到狀態機 */
void start_game(int room_number) {
    // 初始化
    room_state[room_number].current_round = 1;
    room_state[room_number].total_rounds  = room_rounds[room_number]; // 例如3
    room_state[room_number].current_step  = 0; // 還沒開始 step
    room_state[room_number].reply_count   = 0;
    room_state[room_number].step_status   = 0; // 可自定義，如 WAITING_FOR_EVENT=0
    room_state[room_number].is_game_over  = 0;
    room_state[room_number].leftcoin      = 0;
    for (int i = 0; i < 5; i++) {
        room_state[room_number].tragedy[i] = 0;
        room_state[room_number].appear[i] = 0;
    }

    char sendline[MAXLINE];
    int count = room_client_count[room_number];
    room_state[room_number].active_player_count = count;
    for (int i = 0; i < count; i++) {
        room_state[room_number].decisions[i] = '\0'; // 尚未決定
        room_state[room_number].scores[i]    = 0; 
        room_state[room_number].tempcoin[i] = 0;   
    }
    proceed_next_step(room_number);
}

/* 處理客戶端訊息主函式 */
void handle_client_message(int client_fd, fd_set *all_fds, int *max_fd) {
    static int stage[MAX_CLIENTS];  // 每個 client_fd 有個 stage
    if (stage[client_fd] == 0 && client_names[client_fd][0] == '\0') {
        stage[client_fd] = STAGE_INPUT_NAME;
    }

    char sendline[MAXLINE], readline[MAXLINE];
    int bytes_read = read(client_fd, readline, sizeof(readline) - 1);

    if (bytes_read <= 0) {
        handle_client_disconnect(client_fd, all_fds);
        return;
    }

    readline[bytes_read] = '\0';  // 字串結尾
    printf("Received from client FD %d: %s\n", client_fd, readline);
    printf("stage: %d\n", stage[client_fd]);

    int room_number = client_rooms[client_fd];

    switch (stage[client_fd]) {
    /* --------------- 輸入姓名階段 --------------- */
    case STAGE_INPUT_NAME:
        readline[strcspn(readline, "\n")] = '\0'; // 移除換行
        strncpy(client_names[client_fd], readline, MAX_NAME_LENGTH - 1);
        client_names[client_fd][MAX_NAME_LENGTH - 1] = '\0';

        snprintf(sendline, sizeof(sendline), 
                 "Welcome, %s! Enter room number (1-%d):\n", 
                 client_names[client_fd], MAX_ROOMS);
        write(client_fd, sendline, strlen(sendline));

        stage[client_fd] = STAGE_SELECT_ROOM;
        break;

    /* --------------- 選擇房間階段 --------------- */
    case STAGE_SELECT_ROOM: {
        int chosen_room = atoi(readline) - 1;
        if (chosen_room < 0 || chosen_room >= MAX_ROOMS) {
            snprintf(sendline, sizeof(sendline), 
                     "Invalid room number. Please enter a number between 1 and %d:\n", 
                     MAX_ROOMS);
            write(client_fd, sendline, strlen(sendline));
            break;
        }

        // 如果房間還沒設定 max_players 就是 0，表示正在設定中，可以加入
        // 或者 room_ready==0 也表示尚未設定完成 (視你的邏輯)
        // 下面這段你可以依實際邏輯做調整
        if (room_ready[chosen_room] == 0 && room_client_count[chosen_room] > 0) {
            // 代表有房主在設置
            snprintf(sendline, sizeof(sendline), 
                     "Room %d is being set up. Try another room or wait.\n", 
                     chosen_room + 1);
            write(client_fd, sendline, strlen(sendline));
            break;
        }

        // 如果房間已滿
        if (room_client_count[chosen_room] >= MAX_CLIENTS_PER_ROOM) {
            snprintf(sendline, sizeof(sendline), 
                     "Room %d is full. Try another room.\n", chosen_room + 1);
            write(client_fd, sendline, strlen(sendline));
            break;
        }

        // 加入房間
        room_clients[chosen_room][room_client_count[chosen_room]] = client_fd;
        strncpy(room_client_names[chosen_room][room_client_count[chosen_room]], 
                client_names[client_fd], MAX_NAME_LENGTH - 1);
        room_client_names[chosen_room]
            [room_client_count[chosen_room]][MAX_NAME_LENGTH - 1] = '\0';

        room_client_count[chosen_room]++;
        client_rooms[client_fd] = chosen_room;
        room_number = chosen_room;

        printf("client_room:%d  chosen_room: %d  room_number: %d\n", 
               client_rooms[client_fd], chosen_room, room_number);

        // 如果這是該房間第 1 個玩家 -> 成為房主
        if (room_client_count[chosen_room] == 1) {
            room_host[chosen_room] = client_fd;
            snprintf(sendline, sizeof(sendline), 
                     "You are the host of room %d. Set the number of players (5-8):\n", 
                     chosen_room + 1);
            write(client_fd, sendline, strlen(sendline));
            stage[client_fd] = STAGE_SET_PLAYER_COUNT;
        } else {
            // 其他玩家 -> 客人
            int guest_number = room_client_count[chosen_room];
            snprintf(sendline, sizeof(sendline),
                     "You are guest no.%d in room %d.\nWaiting for the host to set up the game.\n",
                     guest_number, chosen_room + 1);
            write(client_fd, sendline, strlen(sendline));

            // 廣播新玩家加入
            snprintf(sendline, sizeof(sendline),
                     "%s has joined room %d as guest no.%d.\n", 
                     client_names[client_fd], chosen_room + 1, guest_number);
            broadcast_message(chosen_room, sendline, client_fd);

            stage[client_fd] = STAGE_ROOM_OPERATION;
        }

        /***********************************************************
         * (新) 立即檢查人數是否已達上限, 若已滿就廣播
         * 注意：如果房主還沒設定 room_max_players[chosen_room]，這裡可能是 0
         * 所以要確定房主「已經」設定好玩家數(room_max_players)之後，才做這個判斷。
         ***********************************************************/
        if (room_max_players[chosen_room] > 0 &&
            room_client_count[chosen_room] == room_max_players[chosen_room]) 
        {
            snprintf(sendline, sizeof(sendline), 
                     "Room %d is now full (%d/%d)! All players, please type 'ok' or 'ready' to prepare.\n",
                     chosen_room + 1, room_client_count[chosen_room], room_max_players[chosen_room]);
            broadcast_message(chosen_room, sendline, -1);

            // 讓房內所有人的 stage 進入準備階段
            for (int i = 0; i < room_client_count[chosen_room]; i++) {
                int guest_fd = room_clients[chosen_room][i];
                stage[guest_fd] = STAGE_READY;
            }
            // 標記房間已進入「要準備」的階段
            room_ready[chosen_room] = 2;
        }
        break;
    }

    /* --------------- 設置玩家數階段(房主) --------------- */
    case STAGE_SET_PLAYER_COUNT: {
        long value = atol(readline);
        if (value >= 5 && value <= 8) {
            room_max_players[room_number] = (int)value;
            snprintf(sendline, sizeof(sendline),
                     "Player numbers set to %d. Please enter the round numbers (3-5):\n",
                     room_max_players[room_number]);
            write(client_fd, sendline, strlen(sendline));
            stage[client_fd] = STAGE_SET_ROUNDS;
        } else {
            snprintf(sendline, sizeof(sendline), 
                     "Invalid input. Please enter a number between 5 and 8:\n");
            write(client_fd, sendline, strlen(sendline));
        }
        break;
    }

    /* --------------- 設置回合數階段(房主) --------------- */
    case STAGE_SET_ROUNDS: {
        long value = atol(readline);
        if (value >= 3 && value <= 5) {
            room_rounds[room_number] = (int)value;
            room_ready[room_number]  = 1;

            snprintf(sendline, sizeof(sendline), 
                     "Round numbers set to %ld. Room setup complete! Wait for the players joining\n", value);
            write(client_fd, sendline, strlen(sendline));
            broadcast_message(room_number, sendline, client_fd);

            stage[client_fd] = STAGE_ROOM_OPERATION;

            // 若現在人數已經到達了上限，就立即通知準備
            if (room_client_count[room_number] == room_max_players[room_number]) {
                snprintf(sendline, sizeof(sendline),
                         "Room %d is now full! All players, please type 'ok' or 'ready' to prepare.\n",
                         room_number + 1);
                broadcast_message(room_number, sendline, -1);

                for (int i = 0; i < room_client_count[room_number]; i++) {
                    int guest_fd = room_clients[room_number][i];
                    stage[guest_fd] = STAGE_READY;
                }
                room_ready[room_number] = 2; 
            }
        } else {
            snprintf(sendline, sizeof(sendline),
                     "Invalid input. Please enter a number between 3 and 5:\n");
            write(client_fd, sendline, strlen(sendline));
        }
        break;
    }

    /* --------------- 房間操作階段 --------------- */
    case STAGE_ROOM_OPERATION: {
        // 如果是房主，可能在這裡檢查一些命令，如「開始遊戲」之類
        if (client_fd == room_host[room_number]) {
            // 你可以在這裡或別處加上命令判斷
            // e.g. if (strcasecmp(readline, "start") == 0) start_game(room_number);
        } else {
            // 其他玩家的訊息就廣播給房間
            snprintf(sendline, sizeof(sendline), "%s: %s\n",
                     client_names[client_fd], readline);
            broadcast_message(room_number, sendline, client_fd);
        }

        // (舉例) 檢查如果人數到齊 => 全部STAGE_READY
        if (room_max_players[room_number] > 0 &&
            room_client_count[room_number] == room_max_players[room_number] &&
            room_ready[room_number] == 1) 
        {
            snprintf(sendline, sizeof(sendline),
                     "Room %d is now full! All players, please type 'ok' or 'ready' to prepare.\n",
                     room_number + 1);
            broadcast_message(room_number, sendline, -1);

            for (int i = 0; i < room_client_count[room_number]; i++) {
                int guest_fd = room_clients[room_number][i];
                stage[guest_fd] = STAGE_READY;
            }
            room_ready[room_number] = 2;
        }

        break;
    }

    /* --------------- 準備階段 --------------- */
    case STAGE_READY: {
        // 移除換行
        readline[strcspn(readline, "\n")] = '\0';

        if (strcasecmp(readline, "ok") == 0 || strcasecmp(readline, "ready") == 0) {
            // 找到 client 在房間中的索引
            int index = -1;
            for (int i = 0; i < room_client_count[room_number]; i++) {
                if (room_clients[room_number][i] == client_fd) {
                    index = i;
                    break;
                }
            }
            if (index != -1) {
                ready_status[room_number][index] = 1;
                snprintf(sendline, sizeof(sendline), "%s is ready.\n",
                         client_names[client_fd]);
                broadcast_message(room_number, sendline, -1);
            }

            // 檢查是否全員都 ready
            int all_ready = 1;
            for (int i = 0; i < room_client_count[room_number]; i++) {
                if (ready_status[room_number][i] == 0) {
                    all_ready = 0;
                    break;
                }
            }
            if (all_ready) {
                snprintf(sendline, sizeof(sendline), 
                         "All players are ready. Host, type 'gogo' to start the game.\n");
                broadcast_message(room_number, sendline, -1);
            }
        } 
        else if (client_fd == room_host[room_number] && strcasecmp(readline, "gogo") == 0) {
            snprintf(sendline, sizeof(sendline), "Game starting now!\n");
            broadcast_message(room_number, sendline, -1);

            snprintf(sendline, sizeof(sendline), "Game in room %d has started! Prepare to explore!\n",
                     room_number + 1);
            broadcast_message(room_number, sendline, -1);

            remaining_players[room_number] = room_client_count[room_number];
            room_ready[room_number] = 3;

            // 切換到 STAGE_RUNNING
            for (int i = 0; i < room_client_count[room_number]; i++) {
                ready_status[room_number][i] = 0;
                stage[room_clients[room_number][i]] = STAGE_RUNNING;
            }
            
            start_game(room_number);
        }
        else {
            snprintf(sendline, sizeof(sendline), 
                     "Invalid command. Type 'ok' or 'ready'.\n");
            write(client_fd, sendline, strlen(sendline));
        }
        break;
    }

    /* --------------- 遊戲進行階段 --------------- */
    case STAGE_RUNNING: {
        readline[strcspn(readline, "\n")] = '\0';
        RoomState *st = &room_state[room_number];
        if (st->step_status == 1) { 
            // 1代表 WAITING_FOR_DECISION
            // 取得該玩家在房間內的 index
            int index = -1;
            for (int i = 0; i < room_client_count[room_number]; i++) {
                if (room_clients[room_number][i] == client_fd) {
                    index = i; 
                    break;
                }
            }
            
            // 若該玩家已經回覆過，避免重複計算
            // if (st->decisions[index] != '\0') {
            //     write(client_fd, "You have already decided.\n", 27);
            //     return;
            // }

            // 檢查 readline 是否 y/n
            if (strcasecmp(readline, "y") == 0) {
                st->decisions[index] = 'Y';
                st->reply_count++;
            } 
            else if (strcasecmp(readline, "n") == 0) {
                st->decisions[index] = 'N';
                st->reply_count++;
            } 
            else {
                write(client_fd, "Invalid input. Type 'y' or 'n'.\n", 32);
                return;
            }

            // 廣播誰做了決定
            // char buf[64];
            // snprintf(buf, sizeof(buf), "%s decided: %s\n", 
            //          client_names[client_fd], 
            //          (st->decisions[index] == 'Y' ? "go home":"stay"));
            // broadcast_message(room_number, buf, -1);

            // 如果所有「還在探險的玩家」都回覆了，或 st->reply_count == st->active_player_count
            if (st->reply_count == st->active_player_count) {
                // 全員回覆完成 -> 進行結算
                finalize_step_decisions(room_number);
            } 
            else {
                // 尚未全員回覆 -> 等其他玩家 or 超時
                // 可以考慮在這裡做 nothing，繼續等
            }
        }
    
        break;
    }

    default:
        // 未定義的階段 -> 中斷連線 (或回到輸入階段)
        snprintf(sendline, sizeof(sendline), "Unknown stage. Disconnecting.\n");
        write(client_fd, sendline, strlen(sendline));
        handle_client_disconnect(client_fd, all_fds);
        break;
    }
}

/* --------------- main --------------- */
int main() {
    int server_fd, client_fd, max_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    fd_set all_fds, read_fds;
    FD_ZERO(&all_fds);

    initialize_rooms(); // 初始化房間資訊

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

    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(SERV_PORT + 5);

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

                    // 送初始訊息
                    const char *welcome_msg = "Enter your name:\n";
                    if (write(client_fd, welcome_msg, strlen(welcome_msg)) < 0) {
                        perror("Write to client failed");
                    }
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
