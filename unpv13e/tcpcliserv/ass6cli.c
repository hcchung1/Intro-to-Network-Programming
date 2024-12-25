#include	"unp.h"
#include  <string.h>
#include  <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

char id[MAXLINE];

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define TIMEOUT_SEC 20

extern void* create_window(int width, int height, const char* title);
extern void clear_window(void* window, int r, int g, int b);
extern void display_window(void* window);
extern void close_window(void* window);
extern void* create_text(void* window, const char* font_path, const char* text, int size, int x, int y, int r, int g, int b);
extern void draw_text(void* window, void* text);
extern void delete_text(void* text);
extern void poll_events(void* window);
extern void* load_image(const char* image_path);
extern void draw_image(void* window, void* image, int x, int y);
extern void delete_image(void* image);
extern void draw_rectangle(void* window, int x, int y, int width, int height, int r, int g, int b);
extern void* create_button(int x, int y, int width, int height, int r, int g, int b);
extern void draw_button(void* window, void* button);
extern void delete_button(void* button);
extern int is_button_clicked(void* window, void* button, int mouse_x, int mouse_y);
extern int get_mouse_position_x(void* window);
extern int get_mouse_position_y(void* window);
extern int is_mouse_button_pressed();
extern void* detect_clicked_button(void* window, int mouse_x, int mouse_y);
extern void* create_circle_button(int x, int y, int radius, int r, int g, int b);
extern void draw_circle_button(void* window, void* button);
extern void delete_circle_button(void* button);
extern int is_circle_button_clicked(void* window, void* button, int mouse_x, int mouse_y);
extern bool has_focus(void* window);
extern int is_key_pressed();
extern char get_pressed_key();
// extern const char* get_input_text(void* window);
// extern void clear_input_text(void* window);
// extern void draw_input_text(void* window, const char* font_path, int size, int x, int y, int r, int g, int b);
// extern int is_enter_pressed(void* window);
// extern void capture_text_input_with_enter(void* window);
// extern const char* get_input_text_with_enter(void* window);



// 定義房間結構
typedef struct Room {
    char name[50];            // 房間名稱
	char password[50];        // 房間密碼，all 0 means no password
    // char description[256];    // 房間描述
    int adjacentRooms[5];     // 鄰接房間的索引
    int hasTreasure;          // 是否有寶藏 (1 表示有，0 表示無)
    int enemies;              // 房間中的敵人數量
    char items[5][50];        // 道具清單 (最多 5 個道具)
    int visited;              // 是否已訪問過 (1 表示已訪問，0 表示未訪問)
	bool isFull;              // 房間是否已滿 (1 表示已滿，0 表示未滿)

} Room;

// 最大房間數量
#define MAX_ROOMS 5

/* the following two functions use ANSI Escape Sequence */
/* refer to https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797 */

// Hello
// Hello2

void clr_scr() {
	printf("\x1B[2J");
};

void set_scr() {		// set screen to 80 * 25 color mode
	printf("\x1B[=3h");
};

void errormsg(char *msg) {
	perror(msg);
	exit(0);
};

int stage = 0;

typedef struct Host {
	bool ishost;
	bool ishosted;
	bool nply_ing;
	bool nply_ed;
	bool nround_ing;
	bool nround_ed;
	int player_num;
	int round_num;
}Host;

typedef struct Obs{
	int wh;
	int num;
	/*
	 * 1: Gem
	 * 2: Tragedy1: Snake
	 * 3: Tragedy2: Rocks
	 * 4: Tragedy3: Fire
	 * 5: Tragedy4: Spiders
	 * 6: Tragedy5: Zombies
	 * 7: Treasure
	 */
}Obs;

typedef struct Stri{
	char str[256];
}Stri;

typedef struct PP{
	char name[128];
	int score;
}PP;

typedef struct Screen {
	char name[128];
	bool name_ing;
	bool name_ed;
	bool room_ing;
	bool room_ed;
	Host host;
	int room_num; // decide which room to join
	int player_ch; // player's choice
	bool player_ch_ing; // player's choice is in progress
	bool player_ch_ed; // player's choice is done
	bool result_ing; // result is in progress
	bool isready; // player is ready (for guest), start the game (for host)
	bool gamestart; // game is started(broadcasted by server)
	int round_now; // current round number
	int step_now; // current step number
	bool next_ing; 
	bool next_ed; 
	Obs obs;
	bool gameover;
	int all_room_num[5];
	Stri msgs[8];
	int gem_num;
	PP players[10];
	Stri stay[10];
	bool startmsg;
	bool rd_startmsg[5];
} screen;

screen scr;

void split_text_into_lines(const char* text, int max_chars, char lines[][256], int* line_count) {
    *line_count = 0;
    int len = strlen(text);
    for (int i = 0; i < len; i += max_chars) {
        strncpy(lines[*line_count], text + i, max_chars);
        lines[*line_count][max_chars] = '\0';
        (*line_count)++;
    }
}

void xchg_data(FILE *fp, int sockfd, void* window, void* image)
{
    int       maxfdp1, stdineof, peer_exit, n;
    fd_set    rset;
    char      sendline[MAXLINE], recvline[MAXLINE];
	char input_buffer[128] = "";
	int input_len = 0;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;
	// stage 0: ID
	// stage 1: send ID
	// stage 2: room
	// stage 3: chosen room
	// stage 4: make command
	// stage 5: waiting for the result

	bzero(sendline, MAXLINE);
	bzero(recvline, MAXLINE);

	bool first = false;
	peer_exit = 0;
	stdineof = 0;
	clear_window(window, 0, 0, 0);  // 黑色背景
	draw_image(window, image, (WINDOW_WIDTH - 700) / 2, 50); // 圖片放置於中央偏上
	display_window(window);  // 顯示內容
	bool ms_ps = false;
	bool key_ps = false;

    for ( ; ; ) {	
		// if(!first) {printf("firsttime\n"); first = true;};

		poll_events(window);
		
		FD_ZERO(&rset);
		maxfdp1 = 0;
        if (stdineof == 0) {
            FD_SET(fileno(fp), &rset);
			maxfdp1 = fileno(fp);
		};	
		if (peer_exit == 0) {
			FD_SET(sockfd, &rset);
			if (sockfd > maxfdp1)
				maxfdp1 = sockfd;
		};	
        maxfdp1++;
        Select(maxfdp1, &rset, NULL, NULL, &timeout); // timeout for 0 seconds
		if (FD_ISSET(sockfd, &rset)) {  /* socket is readable */
			n = read(sockfd, recvline, MAXLINE);
			if (n == 0) {
 		   		if (stdineof == 1)
                    return;         /* normal termination */
		   		else {
					errormsg("server terminated prematurely");
				};
            }
			else if (n > 0) {
				// change here to know everyting about the room
				char *message;
				recvline[n] = '\0';
				message = strtok(recvline, "\n"); // 按換行符分割訊息

				// printf("recv: %s\n", recvline);
				char cmp0[256];
				int guest_number, chosen_room, step, nums, round;
				int nums1 = 0, nums2 = 0, nums3 = 0, nums4 = 0, nums5 = 0;
				char msgchat[256];
				char msgchat2[256];
				sprintf(msgchat2, "Round %d ended.", scr.round_now);

				while (message != NULL) {

					if(strcmp(message, "Enter your name:") == 0){
						// printf("Enter your name: \n");
						scr.name_ing = true;
						for(int i = 0; i < 5; i++){
							scr.all_room_num[i] = 0;
						}

					} 
					else if(strcmp(message, msgchat2) == 0){
						// printf("Round %d ed\n", scr.round_now);
						scr.next_ing = true;
						scr.next_ed = false;
					}
					else if(sscanf(message, "Welcome, %s! Enter room number (1-5):", &cmp0) == 1){
						cmp0[strlen(cmp0)-1] = '\0';
						if(strcmp(cmp0, scr.name) != 0)	errormsg("name not match");
						// printf("Enter room number (1-5): \n");
						// printf("%s\n", message);
						for(int i = 0; i < 5; i++){
							message = strtok(NULL, "\n");
							if(sscanf(message, "%d", &nums) == 1){
								scr.all_room_num[i] = nums;
							}
						}
						// printf("room number: %d %d %d %d %d\n", scr.all_room_num[0], scr.all_room_num[1], scr.all_room_num[2], scr.all_room_num[3], scr.all_room_num[4]);
						scr.room_ing = true;

					}
					else if(sscanf(message, "You are the host of room %d. Set the number of players (5-8):", &chosen_room) == 1){
						if(chosen_room != scr.room_num) errormsg("room number not match");
						// printf("You are the host!!\n");
						// printf("Pick the number of players now!\n");
						scr.host.nply_ing = true;
						scr.host.ishost = true;
						scr.host.ishosted = true;

					}
					else if(sscanf(message, "Player numbers set to %d. Please enter the round numbers (3-5):", &nums) == 1){
						if(nums != scr.host.player_num) errormsg("player number not match");
						// printf("Player numbers set to %d. Please enter the round numbers (3-5):\n", nums);
						scr.host.nround_ing = true;
						scr.host.nround_ed = false;
						scr.host.nply_ed = true;

					}
					else if(sscanf(message, "Round numbers set to %d. Room setup complete! Wait for the players joining\n", &nums) == 1){
						if(nums != scr.host.round_num) errormsg("round number not match");
						// printf("Round numbers set to %d. Room setup complete! Wait for the players joining\n", nums);
						scr.host.nround_ed = true;

					}
					else if (sscanf(message, "You are guest in room %d.", &chosen_room) == 1) {
						// 匹配成功，表示這是一個客人收到的消息
						// printf("You are the guest in room %d.\n", chosen_room);
						scr.room_num = chosen_room; // 記錄房間號
						scr.host.ishost = false;        // 表示不是房主
						scr.host.ishosted = true;

					} 
					else if(strcmp(message, "Waiting for the host to set up the game.") == 0){
						// printf("Waiting for the host to set up the game.\n");

					}
					else if(sscanf(message, "Game in room %d has started! Prepare to explore!", &chosen_room) == 1){
						// printf("Game in room %d has started! Prepare to explore!!\n", chosen_room);
						if(chosen_room == scr.room_num)scr.gamestart = true;
						else errormsg("room number not match");

					} 
					else if(sscanf(message, "Round %d start from NOW!", &round) == 1){
						// printf("Round %d starts!!\n", round);
						scr.round_now = round;
						scr.next_ed = true;
						scr.next_ing = false;
						// clear scr.players[10]
						for(int i = 0; i < 10; i++){
							scr.players[i].name[0] = '\0';
							scr.players[i].score = 0;
						}

					}
					else if(sscanf(message, "Room %d is now full! All players, please type 'ok' or 'ready' to prepare.\n", &nums) == 1){
						if(nums != scr.room_num) errormsg("room number not match");
						if(!scr.host.ishost)scr.startmsg = true;
						// printf("Room %d is now full! All players, please type 'ok' or 'ready' to prepare.\n", nums);
					}
					else if(sscanf(message, "Step:%d", &step) == 1){
						// printf("Step: %d\n", step);
						scr.step_now = step;						

					}
					else if(sscanf(message, "----Discovered %d gems!----", &nums) == 1){
						// printf("Discovered %d gems!\n", nums);
						scr.obs.wh = 1;
						scr.obs.num = nums;

					}
					else if(strcmp(message, "----TRAGEDY1:SNAKE----") == 0){
						// printf("tragedy: snake\n");
						scr.obs.wh = 2;
						scr.obs.num = 1;

					} 
					else if(strcmp(message, "----TRAGEDY2:ROCKS----") == 0){
						// printf("tragedy: rocks\n");
						scr.obs.wh = 3;
						scr.obs.num = 1;

					}
					else if(strcmp(message, "----TRAGEDY3:FIRE----") == 0){
						// printf("tragedy: fire\n");
						scr.obs.wh = 4;
						scr.obs.num = 1;

					}
					else if(strcmp(message, "----TRAGEDY4:SPIDERS----") == 0){
						// printf("tragedy: spiders\n");
						scr.obs.wh = 5;
						scr.obs.num = 1;

					}
					else if(strcmp(message, "----TRAGEDY5:ZOMBIES----") == 0){
						// printf("tragedy: zombies\n");
						scr.obs.wh = 6;
						scr.obs.num = 1;

					}
					else if(sscanf(message, "%d gem(s) left on the floor.\n", &nums) == 1){
						// printf("%d gem(s) left on the floor.\n", nums);
						scr.gem_num = nums;
						// printf("gem num: %d\n", scr.gem_num);

					}
					else if(sscanf(message, "WOW! It's a treasure!!! Value: {%d}.", &nums) == 1){
						// printf("WOW! It's a treasure!!! Value: {%d}.\n", nums);
						scr.obs.wh = 7;
						scr.obs.num = nums;

					}
					else if(strcmp(message, "All players are ready. Host, type 'gogo' to start the game.") == 0){
						// printf("All players are ready. Type 'gogo' to start the game.\n");
						if(scr.host.ishost) scr.startmsg = true;

					}
					else if(strcmp(message, "All players are ready.") == 0){
						if(scr.host.ishost) scr.rd_startmsg[scr.round_now] = true;
					}
					else if(strcmp(message, "All rounds completed. Game Over!") == 0){
						// printf("All rounds completed. Game Over!\n");
						scr.gameover = true;

					}
					else if(strcmp(message, "NOW, it's time to make a decision...Do you want to go home or stay? (y/n)") == 0){
						// printf("NOW, it's time to make a decision...Do you want to go home or stay? (y/n)\n");
						scr.player_ch_ing = true;
						scr.player_ch_ed = false;

					}
					else if(strcmp(message, "All rounds done! Game Over!") == 0){
						// printf("Game Over!\n");
						scr.gameover = true;
						scr.next_ing = false;
						scr.next_ed = false;

					}
					else if(strcmp(message, "msg") == 0){
						// printf("kk\n\n");
						message = strtok(NULL, "\n");
						strcpy(msgchat, message);
						// printf("msg: %s\n", msgchat);
						message = strtok(NULL, "\n");
						strcpy(cmp0, message);
						// printf("cmp0: %s\n", cmp0);
						bool msgfull = true;
						char msgtmp[256];
						if(strcmp(msgchat, scr.name) != 0){
							sprintf(msgtmp, "%s: %s", msgchat, cmp0);
						}
						else sprintf(msgtmp, "me: %s",cmp0);
						for(int i = 0; i < 8; i++){
							if(scr.msgs[i].str[0] == '\0'){
								strcpy(scr.msgs[i].str, msgtmp);
								msgfull = false;
								break;
							}
						}
						if(msgfull){
							// if msg is full, shift the msg
							for(int i = 7; i > 0; i--){
								// copy the msgs[i] to msgs[i-1]
								strcpy(scr.msgs[i-1].str, scr.msgs[i].str);
							}
							strcpy(scr.msgs[7].str, msgtmp);
						}

					}
					else if(strcmp(message, "All players went home, end of this round.") == 0){
						// printf("All players went home, end of this round.\n");
						scr.next_ing = true;
						scr.next_ed = false;

					}
					else if(strcmp(message, "REMEMBER: ONLY ONE person can bring it back...") == 0) nums = 0; // no use
					else if(strcmp(message, "start next round: please type ok to prepare.") == 0) {
						if(!scr.host.ishost) scr.rd_startmsg[scr.round_now] = true;
					}
					else if(sscanf(message, "%s 's total scores: %d", &cmp0, &nums) == 2){
						// printf("%s 's total scores: %d\n", cmp0, nums);
					}
					else if(sscanf(message, "%s went home with score: %d", &cmp0, &nums) == 2){
						// printf("%s w %d\n", cmp0, nums);
						for(int i = 0; i < 10; i++){
							if(strcmp(scr.players[i].name, cmp0) == 0){
								scr.players[i].score = nums;
								break;
							}
							if(scr.players[i].name[0] == '\0'){
								strcpy(scr.players[i].name, cmp0);
								scr.players[i].score = nums;
								break;
							}
						}
					}
					else if(sscanf(message, "went home with nothing: %s\n", &cmp0) == 1){
						// printf("%s w nothing\n", cmp0);
						for(int i = 0; i < 10; i++){
							if(strcmp(scr.players[i].name, cmp0) == 0){
								scr.players[i].score = 0;
								break;
							}
							if(scr.players[i].name[0] == '\0'){
								strcpy(scr.players[i].name, cmp0);
								scr.players[i].score = 0;
								break;
							}
						}
					}
					else if(sscanf(message, "decided to stay: %s\n", &cmp0) == 1){
						// printf("%s stay\n", cmp0);
						for(int i = 0; i < 10; i++){
							if(scr.stay[i].str[0] == '\0'){
								strcpy(scr.stay[i].str, cmp0);
								printf("at %d\n", i);
								break;
							}
						}
					}
					else if(strcmp(message, "Other players are making choices...") == 0) nums = 0; // no use
					else if(sscanf(message, "The new host of room %d is %s\n", &nums, &cmp0) == 2){
						if(nums != scr.room_num) errormsg("room number not match");
						else{
							if(strcmp(cmp0, scr.name) == 0){
								scr.host.ishost = true;
								scr.host.ishosted = true;
								scr.host.player_num = 1;
								scr.host.round_num = 1;
								// printf("You are the host now!\n");
								char msgtmp[256];
								sprintf(msgtmp, "sys: You are the host now!");
								bool msgfull = true;
								for(int i = 0; i < 8; i++){
									if(scr.msgs[i].str[0] == '\0'){
										strcpy(scr.msgs[i].str, msgtmp);
										msgfull = false;
										break;
									}
								}
								if(msgfull){
									// if msg is full, shift the msg
									for(int i = 7; i > 0; i--){
										// copy the msgs[i] to msgs[i-1]
										strcpy(scr.msgs[i-1].str, scr.msgs[i].str);
									}
									strcpy(scr.msgs[7].str, msgtmp);
								}
							}else{
								char msgtmp[256];
								sprintf(msgtmp, "sys: New host is %s", cmp0);
								bool msgfull = true;
								for(int i = 0; i < 10; i++){
									if(scr.msgs[i].str[0] == '\0'){
										strcpy(scr.msgs[i].str, msgtmp);
										msgfull = false;
										break;
									}
								}
								if(msgfull){
									// if msg is full, shift the msg
									for(int i = 7; i > 0; i--){
										// copy the msgs[i] to msgs[i-1]
										strcpy(scr.msgs[i-1].str, scr.msgs[i].str);
									}
									strcpy(scr.msgs[7].str, msgtmp);
								}
							}
							
						}
						
					}
					else if(strcmp(message, "Game starting now!") == 0) nums = 0; // no use
					else {
						char msgtmp[256];
						sprintf(msgtmp, "sys: %s", message);
						bool msgfull = true;
						for(int i = 0; i < 8; i++){
							if(scr.msgs[i].str[0] == '\0'){
								strcpy(scr.msgs[i].str, msgtmp);
								msgfull = false;
								break;
							}
						}
						if(msgfull){
							// if msg is full, shift the msg
							for(int i = 7; i > 0; i--){
								// copy the msgs[i] to msgs[i-1]
								strcpy(scr.msgs[i-1].str, scr.msgs[i].str);
							}
							strcpy(scr.msgs[7].str, msgtmp);
						}
						
					}

					
					// 獲取下一條訊息
					message = strtok(NULL, "\n");
				}
				
				
			}
			else { // n < 0
			    printf("(server down)");
				return;
			};
			bzero(recvline, MAXLINE);
			bzero(sendline, MAXLINE);
        }
		
        if (FD_ISSET(fileno(fp), &rset)) {  /* input is readable */

			// printf("something gets from stdin1\n");

            if (Fgets(sendline, MAXLINE, fp) == NULL) {
				if (peer_exit)
					return;
				else {
					printf("(leaving...)\n");
					stdineof = 1;
					Shutdown(sockfd, SHUT_WR);      /* send FIN */
				};
            }else {
				
				sendline[strlen(sendline)-1] = '\0';
				Writen(sockfd, sendline, strlen(sendline));
				bzero(sendline, MAXLINE);
				bzero(recvline, MAXLINE);
			};
        }

		
		// 清除背景並繪製內容
        clear_window(window, 0, 0, 0);  // 黑色背景
		bool someonehome = false;
		for(int i = 0; i < 10; i++){
			if(scr.players[i].name[0] != '\0'){
				someonehome = true;
				break;
			}
		}
        if(!someonehome)draw_image(window, image, (WINDOW_WIDTH - 700) / 2, 0); // 圖片放置於中央偏上
		else{
			int count = 0;
			int x = 10;
			int y = 10;
			for(int i = 0; i < 10; i++){
				if(scr.players[i].name[0] != '\0'){
					char tmp[256];
					sprintf(tmp, "%s: %d", scr.players[i].name, scr.players[i].score);
					void* text = create_text(window, "Arial.ttf", tmp, 20, x, y, 255, 255, 255);
					draw_text(window, text);
					delete_text(text);
					if(count % 2 == 0){
						y = 40;
					}else{
						x += 150;
						y = 10;
					}
					count++;
				}
			}
		}

		if(scr.name_ing){

			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "Enter your name:(press Enter to send)"; // 標題文字
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (800 - text_width) / 2, (600 - font_size) / 2,
									  255, 255, 255);
			if (!title) errormsg("create_text failed");
			draw_text(window, title);
			delete_text(title);

			// 創建並繪製輸入框
			void* button = create_button(190, 345, 390, 50, 255, 255, 255); // 白色background
			draw_button(window, button);
			delete_button(button);
			button = create_button(195, 350, 380, 40, 0, 0, 0); // 黑色邊框
			draw_button(window, button);
			delete_button(button);


			void* input_text = create_text(window, font_path, input_buffer, 30, 200, 350, 255, 255, 255);
			draw_text(window, input_text);
			delete_text(input_text);

			// 處理輸入事件
			if (has_focus(window)) {
				// poll_events(window);
				char c;
				if(is_key_pressed() == true && key_ps == false){
					key_ps = true;
					c = get_pressed_key();
				}
				if (is_key_pressed() == false && key_ps == true) {
					key_ps = false;
					if (c == '\b' && input_len > 0) {  // 處理刪除鍵
						input_buffer[--input_len] = '\0';
					} 
					else if (c != '\b' && input_len < sizeof(input_buffer) - 1 && c >= 32 && c <= 126) {  // 處理可見字符
						input_buffer[input_len++] = c;
						input_buffer[input_len] = '\0';
					} else if(c == 13 && input_len > 0){
						// printf("Enter pressed\n");
						strncpy(scr.name, input_buffer, sizeof(scr.name) - 1);
						scr.name[strlen(scr.name)] = '\0';
						scr.name_ed = true;
						scr.name_ing = false;
						Writen(sockfd, scr.name, strlen(scr.name));
						// printf("Name sent: %s\n", scr.name);
						bzero(input_buffer, sizeof(input_buffer));
						input_len = 0;

					} 
					else {
						int nn = c;
						printf("Invalid input: %d\n", nn);
					}
					
				}
			}
		}
		
		if (scr.room_ing) {
			const char* font_path = "Arial.ttf";
			const char* title_text = "Choose a room number";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2;
			void* title = create_text(window, font_path, title_text, font_size,
									(800 - text_width) / 2, (600 - font_size) / 2 - 100,
									255, 255, 255);
			draw_text(window, title);
			delete_text(title);

			// 創建並繪製 5 個按鈕
			void* buttons[5];
			for (int i = 0; i < 5; i++) {
				buttons[i] = create_button(200 + i * 100, 300, 50, 50, 0, 255, 0); // 綠色按鈕
				draw_button(window, buttons[i]);

				// 繪製按鈕上的數字
				char num[MAXLINE];
				sprintf(num, "%d", i + 1);
				char text[MAXLINE];
				sprintf(text, "%d in", scr.all_room_num[i]);
				void* num_text = create_text(window, font_path, num, 30, 200 + i * 100 + 15, 300 + 10, 255, 0, 255);
				void* text_text = create_text(window, font_path, text, 30, 200 + i * 100 + 5, 300 + 50, 255, 0, 255);
				draw_text(window, text_text);
				delete_text(text_text);
				draw_text(window, num_text);
				delete_text(num_text);
			}

			// 檢測按鈕點擊
			if(has_focus(window) && is_mouse_button_pressed() == true && ms_ps == false){
				ms_ps = true;
			}
			if (is_mouse_button_pressed() == false && ms_ps == true) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);

				for (int i = 0; i < 5; i++) {
					if (is_button_clicked(window, buttons[i], mouse_x, mouse_y)) {
						// printf("Button %d clicked!\n", i + 1);
						sprintf(sendline, "%d", i + 1);
						sendline[strlen(sendline)] = '\0';
						scr.room_num = i + 1; // 記錄被點擊的按鈕編號
						Writen(sockfd, sendline, strlen(sendline));
						// printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
						scr.room_ing = false; // 完成選擇
						scr.room_ed = true;
					}
				}
				
			}
			// 刪除按鈕資源
			for (int i = 0; i < 5; i++) {
				delete_button(buttons[i]);
			}
		}

		if(scr.next_ing && !scr.host.ishost){
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "Ready to Next Round?";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (10), (81),
									  255, 0, 0);
			draw_text(window, title);
			delete_text(title);
			
			const char* title_text2 = "press Ready!";
			text_width = strlen(title_text2) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text2, font_size,
									  10, (81 + font_size + 10),
									  255, 0, 0);
			draw_text(window, title);
			delete_text(title);

			if(scr.rd_startmsg[scr.round_now]){
				title_text = "Ready";			
				text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
				title = create_text(window, font_path, title_text, font_size,
										30, 500 - font_size / 2,
										255, 255, 255);
				// 按照文字大小建立按鈕
				void* button = create_button((0), (500 - font_size / 2), (text_width + 50), (font_size+10), 0, 0, 255); // 藍色按鈕
				draw_button(window, button);
				draw_text(window, title);
				delete_text(title);
				
				// 檢測按鈕點擊
				if(has_focus(window) && is_mouse_button_pressed() && !ms_ps){
					ms_ps = true;
				}
				if (is_mouse_button_pressed() == false && ms_ps) {
					ms_ps = false;
					int mouse_x = get_mouse_position_x(window);
					int mouse_y = get_mouse_position_y(window);
					if (is_button_clicked(window, button, mouse_x, mouse_y)) {
						// printf("Ready clicked!\n");
						sprintf(sendline, "ok");
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						scr.next_ing = false;
						// printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
					}
				}
				delete_button(button);
			}
			
			char title_text3[50];
			const char* image_path;
			switch(scr.obs.wh){
				case 1:
					sprintf(title_text3, "Last befor end: gems");
					// put picture gem.jpg
					image_path = "gem.png"; // 圖片路徑
					break;
				case 2:
					sprintf(title_text3, "Last befor end: SNAKE");
					image_path = "snake.png"; // 圖片路徑
					break;
				case 3:
					sprintf(title_text3, "Last befor end: ROCKS");
					image_path = "rocks.png"; // 圖片路徑
					break;
				case 4:
					sprintf(title_text3, "Last befor end: FIRE");
					image_path = "fire.png"; // 圖片路徑
					break;
				case 5:
					sprintf(title_text3, "Last befor end: SPIDERS");
					image_path = "spider.png"; // 圖片路徑
					break;
				case 6:
					sprintf(title_text3, "Last befor end: ZOMBIES");
					image_path = "zombie.png"; // 圖片路徑
					break;
				case 7:
					sprintf(title_text3, "Last befor end: Treasure");
					char temp[256];
					sprintf(temp, "%d_point.jpg", scr.obs.num);
					image_path = temp; // 圖片路徑
					break;
				default:
					break;
			}

			void* image = load_image(image_path);
			if (!image) {
				errormsg("Failed to load image.\n");
			}
			draw_image(window, image, 300, 200);
			delete_image(image);

			text_width = (strlen(title_text3)-22) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text3, font_size,
								(800 - text_width) / 2 - 150, 150,
								255, 20, 25);
			if (!title) {
				errormsg("Failed to create title text.\n");
			}
			draw_text(window, title);
			delete_text(title);
		}else if(scr.next_ing && scr.host.ishost){
			const char* font_path = "Arial.ttf"; // 字體路徑
			int font_size = 30;
			const char*title_text = "Ready to Next Round?";
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (10), (81),
									  255, 0, 0);
			draw_text(window, title);
			delete_text(title);

			const char* title_text2 = "press Start!";
			text_width = strlen(title_text2) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text2, font_size,
									  10, (81 + font_size + 10),
									  255, 0, 0);
			draw_text(window, title);
			delete_text(title);

			if(scr.rd_startmsg[scr.round_now]){
				title_text = "Start";
				text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
				title = create_text(window, font_path, title_text, font_size,
										30, 500 - font_size / 2,
										255, 255, 255);
				void* button = create_button((0), (500 - font_size / 2), (text_width + 50), (font_size+10), 0, 0, 255); // 藍色按鈕
				draw_button(window, button);
				draw_text(window, title);
				delete_text(title);

				// 檢測按鈕點擊
				if(has_focus(window) && is_mouse_button_pressed() && !ms_ps){
					ms_ps = true;
				}
				if (is_mouse_button_pressed() == false && ms_ps) {
					ms_ps = false;
					int mouse_x = get_mouse_position_x(window);
					int mouse_y = get_mouse_position_y(window);
					if (is_button_clicked(window, button, mouse_x, mouse_y)) {
						// printf("Start clicked!\n");
						sprintf(sendline, "gogo");
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						scr.next_ing= false;
						// printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
					}
				}
				delete_button(button);

			}
			
			char title_text3[50];
			const char* image_path;
			switch(scr.obs.wh){
				case 1:
					sprintf(title_text3, "Last befor end: gems");
					// put picture gem.jpg
					image_path = "gem.png"; // 圖片路徑
					break;
				case 2:
					sprintf(title_text3, "Last befor end: SNAKE");
					image_path = "snake.png"; // 圖片路徑
					break;
				case 3:
					sprintf(title_text3, "Last befor end: ROCKS");
					image_path = "rocks.png"; // 圖片路徑
					break;
				case 4:
					sprintf(title_text3, "Last befor end: FIRE");
					image_path = "fire.png"; // 圖片路徑
					break;
				case 5:
					sprintf(title_text3, "Last befor end: SPIDERS");
					image_path = "spider.png"; // 圖片路徑
					break;
				case 6:
					sprintf(title_text3, "Last befor end: ZOMBIES");
					image_path = "zombie.png"; // 圖片路徑
					break;
				case 7:
					sprintf(title_text3, "Last befor end: Treasure");
					char temp[256];
					sprintf(temp, "%d_point.jpg", scr.obs.num);
					image_path = temp; // 圖片路徑
					break;
				default:
					break;
			}

			void* image = load_image(image_path);
			if (!image) {
				errormsg("Failed to load image.\n");
			}
			draw_image(window, image, 300, 200);
			delete_image(image);

			text_width = (strlen(title_text3)-22) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text3, font_size,
								(800 - text_width) / 2 - 150, 150,
								255, 20, 25);
			if (!title) {
				errormsg("Failed to create title text.\n");
			}
			draw_text(window, title);
			delete_text(title);
		}
		
		if(scr.host.nply_ing && scr.host.ishost){
			// hasn't decided the room number
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "You are the host!!";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (800 - text_width) / 2, (600 - font_size) / 2 - 100,
									  255, 255, 255);
			draw_text(window, title);
			delete_text(title);

			const char* title_text2 = "Pick the number of players now!";
			int text_width2 = strlen(title_text2) * font_size / 2; // 簡單估算文字寬度
			void* title2 = create_text(window, font_path, title_text2, font_size,
									  (800 - text_width2) / 2, (600 - font_size) / 2 - 50,
									  255, 255, 255);
			draw_text(window, title2);
			delete_text(title2);

			// 創建並繪製 4 個按鈕
			void* buttons[4];
			for (int i = 0; i < 4; i++) {
				buttons[i] = create_button(225 + i * 100, 300, 50, 50, 0, 255, 0); // 綠色按鈕
				draw_button(window, buttons[i]);

				// 繪製按鈕上的數字
				char num[2];
				sprintf(num, "%d", i + 5);
				void* num_text = create_text(window, font_path, num, 30, 225 + i * 100 + 15, 300 + 10, 255, 0, 255);
				draw_text(window, num_text);
				delete_text(num_text);
			}

			// 檢測按鈕點擊
			if(has_focus(window) && is_mouse_button_pressed() && !ms_ps){
				ms_ps = true;
			}
			if (is_mouse_button_pressed() == false && ms_ps) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);

				for (int i = 0; i < 4; i++) {
					if (is_button_clicked(window, buttons[i], mouse_x, mouse_y)) {
						// printf("Button %d clicked!\n", i + 5);
						sprintf(sendline, "%d", i + 5);
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						// printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
						scr.host.player_num = i + 5;
						scr.host.nply_ing = false;
					}
				}
			}

			// 刪除按鈕資源
			for (int i = 0; i < 4; i++) delete_button(buttons[i]);
			
		} 
		else if(scr.host.ishost && scr.host.nround_ing){
			// hasn't decided the round number
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "You are the host!!";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (800 - text_width) / 2, (600 - font_size) / 2 - 100,
									  255, 255, 255);
			draw_text(window, title);
			delete_text(title);

			const char* title_text2 = "Pick the number of rounds now!";
			int text_width2 = strlen(title_text2) * font_size / 2; // 簡單估算文字寬度
			void* title2 = create_text(window, font_path, title_text2, font_size,
									  (800 - text_width2) / 2, (600 - font_size) / 2 - 50,
									  255, 255, 255);
			draw_text(window, title2);
			delete_text(title2);

			// 創建並繪製 3 個按鈕
			void* buttons[3];
			for (int i = 0; i < 3; i++) {
				buttons[i] = create_button(250 + i * 100, 300, 50, 50, 0, 255, 0); // 綠色按鈕
				draw_button(window, buttons[i]);

				// 繪製按鈕上的數字
				char num[2];
				sprintf(num, "%d", i + 3);
				void* num_text = create_text(window, font_path, num, 30, 250 + i * 100 + 15, 300 + 10, 255, 0, 255);
				draw_text(window, num_text);
				delete_text(num_text);
			}

			// 檢測按鈕點擊
			if(has_focus(window) && is_mouse_button_pressed() && !ms_ps) ms_ps = true;
			if (is_mouse_button_pressed() == false && ms_ps) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);

				for (int i = 0; i < 3; i++) {
					if (is_button_clicked(window, buttons[i], mouse_x, mouse_y)) {
						// printf("Button %d clicked!\n", i + 3);
						sprintf(sendline, "%d", i + 3);
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						// printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
						scr.host.round_num = i + 3;
						scr.host.nround_ing = false;
					}
				}
			}

			// 刪除按鈕資源
			for (int i = 0; i < 3; i++) delete_button(buttons[i]);
			
		}

		if(scr.gamestart == false && scr.isready == false && scr.host.ishosted == true && (scr.host.ishost == false && scr.room_ed == true)){
			// 放入照片
			const char* image_path = "incan-gold-game.png"; // 圖片路徑
			void* image = load_image(image_path);
			if (!image) errormsg("Failed to load image");
			draw_image(window, image, 0, 81);
			delete_image(image);
			//在右下角放按鈕作為 ready(guest)

			if(scr.startmsg){
				const char* font_path = "Arial.ttf"; // 字體路徑
				const char* title_text = "Ready";
				int font_size = 30;
				int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
				void* title = create_text(window, font_path, title_text, font_size,
										30, 500 - font_size / 2,
										255, 255, 255);
				// 按照文字大小建立按鈕
				void* button = create_button((0), (500 - font_size / 2), (text_width + 50), (font_size+10), 0, 0, 255); // 藍色按鈕
				draw_button(window, button);
				draw_text(window, title);
				delete_text(title);
				// 檢測按鈕點擊
				if(has_focus(window) && is_mouse_button_pressed() && !ms_ps){
					ms_ps = true;
				}
				if (is_mouse_button_pressed() == false && ms_ps) {
					ms_ps = false;
					int mouse_x = get_mouse_position_x(window);
					int mouse_y = get_mouse_position_y(window);
					if (is_button_clicked(window, button, mouse_x, mouse_y)) {
						// printf("Ready clicked!\n");
						sprintf(sendline, "ok");
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						scr.isready = true;
						// printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
					}
				}
			}
							
		} else if(scr.gamestart == false && scr.isready == false && scr.host.ishosted == true && (scr.host.player_num != 0 && scr.host.round_num != 0)){
			//host
			// 放入照片
			const char* image_path = "incan-gold-game.png"; // 圖片路徑
			void* image = load_image(image_path);
			if (!image) errormsg("Failed to load image");
			draw_image(window, image, 0, 81);
			delete_image(image);
			//在右下角放按鈕作為 start(host)

			if(scr.startmsg){
				const char* font_path = "Arial.ttf"; // 字體路徑
				const char* title_text = "Start";
				int font_size = 30;
				int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
				void* title = create_text(window, font_path, title_text, font_size,
										30, 500 - font_size / 2,
										255, 255, 255);
				void* button = create_button((0), (500 - font_size / 2), (text_width + 50), (font_size+10), 0, 0, 255); // 藍色按鈕
				draw_button(window, button);
				draw_text(window, title);
				delete_text(title);
				// 檢測按鈕點擊
				if(has_focus(window) && is_mouse_button_pressed() && !ms_ps){
					ms_ps = true;
				}
				if (is_mouse_button_pressed() == false && ms_ps) {
					ms_ps = false;
					int mouse_x = get_mouse_position_x(window);
					int mouse_y = get_mouse_position_y(window);
					if (is_button_clicked(window, button, mouse_x, mouse_y)) {
						// printf("Start clicked!\n");
						sprintf(sendline, "gogo");
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						scr.isready = true;
						// printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
					}
				}
			}
		}
			
			

		if (scr.player_ch_ing) {
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = " ";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2;
			void* title = create_text(window, font_path, title_text, font_size,
									(800 - text_width) / 2-20, 135,
									255, 255, 255);
			draw_text(window, title);

			// 創建兩個圓形按鈕
			void* leave_button = create_circle_button(400, 250, 60, 200, 0, 0); // 紅色圓形按鈕
			void* stay_button = create_circle_button(400, 425, 60, 0, 200, 0); // 綠色圓形按鈕
			draw_circle_button(window, leave_button);
			draw_circle_button(window, stay_button);

			title_text = "Leave";
			font_size = 20;
			text_width = strlen(title_text) * font_size / 2;
			title = create_text(window, font_path, title_text, font_size,
									400 - text_width / 2, 425 - font_size / 2,
									0, 0, 0);
			draw_text(window, title);

			title_text = "Stay";
			font_size = 20;
			text_width = strlen(title_text) * font_size / 2;
			title = create_text(window, font_path, title_text, font_size,
									400 - text_width / 2, 250 - font_size / 2,
									0, 0, 0);
			draw_text(window, title);

			delete_text(title);

			// 檢測按鈕點擊
			if (has_focus(window) && is_mouse_button_pressed() && !ms_ps) {
				ms_ps = true;
			}
			if (!is_mouse_button_pressed() && ms_ps) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);

				if (is_circle_button_clicked(window, leave_button, mouse_x, mouse_y)) {
					// printf("Leave clicked!\n");
					sprintf(sendline, "n");
					sendline[strlen(sendline)] = '\0';
					Writen(sockfd, sendline, strlen(sendline));
					// printf("sent: %s\n", sendline);
					scr.player_ch_ing = false;
					scr.player_ch_ed = true;

					// clear stay
					for(int i = 0; i < 10; i++){
						bzero(scr.stay[i].str, sizeof(scr.stay[i].str));
						scr.stay[i].str[0] = '\0';
					}
				}
				if (is_circle_button_clicked(window, stay_button, mouse_x, mouse_y)) {
					// printf("Stay clicked!\n");
					sprintf(sendline, "y");
					sendline[strlen(sendline)] = '\0';
					Writen(sockfd, sendline, strlen(sendline));
					// printf("sent: %s\n", sendline);
					scr.player_ch_ing = false;
					scr.player_ch_ed = true;

					// clear stay
					for(int i = 0; i < 10; i++){
						bzero(scr.stay[i].str, sizeof(scr.stay[i].str));
						scr.stay[i].str[0] = '\0';
					}
				}
			}

			// 刪除按鈕資源
			delete_circle_button(leave_button);
			delete_circle_button(stay_button);

			// put who has decide to leave in the place buttom used to be
			int cnt = 0;
			for(int i = 0; i < 10; i++){
				if(scr.stay[i].str[0] != '\0' && strcmp(scr.stay[i].str, scr.name) != 0){
					char title_text[MAXLINE];
					sprintf(title_text, "%s stay", scr.stay[i].str);
					int font_size = 10;
					int text_width = strlen(title_text) * font_size / 2;
					void* title = create_text(window, font_path, title_text, font_size,
											10, 81 + 10 * cnt,
											255, 255, 255);
					draw_text(window, title);
					delete_text(title);
					cnt++;
				}
			}
		}
		if(scr.room_ed){
			// put name on left bottom
			const char* font_path = "Arial.ttf"; // 字體路徑
			// make title_text to be "Room: %d"
			char title_text[50];
			sprintf(title_text, "Room: %d", scr.room_num);
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  650, 600 - font_size*2,
									  255, 255, 255);
			if (!title) errormsg("Failed to create title text.\n");
			draw_text(window, title);
			delete_text(title);
		}

		if(scr.name_ed){
			// put name on left bottom
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = scr.name;
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  20, 600 - font_size*2,
									  255, 255, 255);
			if (!title) errormsg("Failed to create title text.\n");
			draw_text(window, title);
			delete_text(title);
		} 

		if(scr.gamestart && !scr.gameover && !scr.next_ing && scr.next_ed){
			// put the round number and step number on the mid bottom
			const char* font_path = "Arial.ttf"; // 字體路徑
			// make title_text to be "Round: %d, Step: %d"
			char title_text[50];
			sprintf(title_text, "Round: %d, Step: %d", scr.round_now, scr.step_now);
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (800 - text_width) / 2, 600 - font_size*2,
									  255, 255, 255);
			if (!title) errormsg("Failed to create title text.\n");
			draw_text(window, title);
			const char* image_path;
			switch(scr.obs.wh){
				case 1:
					sprintf(title_text, "Discovered %d gems!\ngo back with %d gem(s)", scr.obs.num, scr.gem_num);
					// put picture gem.jpg
					image_path = "gem.png"; // 圖片路徑
					break;
				case 2:
					sprintf(title_text, "TRAGEDY: SNAKE\ngo back with %d gem(s)", scr.gem_num);
					image_path = "snake.png"; // 圖片路徑
					break;
				case 3:
					sprintf(title_text, "TRAGEDY: ROCKS\ngo back with %d gem(s)", scr.gem_num);
					image_path = "rocks.png"; // 圖片路徑
					break;
				case 4:
					sprintf(title_text, "TRAGEDY: FIRE\ngo back with %d gem(s)", scr.gem_num);
					image_path = "fire.png"; // 圖片路徑
					break;
				case 5:
					sprintf(title_text, "TRAGEDY: SPIDERS\ngo back with %d gem(s)", scr.gem_num);
					image_path = "spider.png"; // 圖片路徑
					break;
				case 6:
					sprintf(title_text, "TRAGEDY: ZOMBIES\ngo back with %d gem(s)", scr.gem_num);
					image_path = "zombie.png"; // 圖片路徑
					break;
				case 7:
					sprintf(title_text, "%d points Treasure\ngo back with %d gem(s)", scr.obs.num, scr.gem_num);
					char temp[256];
					sprintf(temp, "%d_point.jpg", scr.obs.num);
					image_path = temp; // 圖片路徑
					break;
				default:
					break;
			}

			void* image = load_image(image_path);
			if (!image) {
				errormsg("Failed to load image.\n");
			}
			draw_image(window, image, 0, 200);
			delete_image(image);

			text_width = (strlen(title_text)-22) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text, font_size,
								(800 - text_width) / 2 - 50, 100,
								255, 20, 25);
			if (!title) {
				errormsg("Failed to create title text.\n");
			}
			draw_text(window, title);
			delete_text(title);
		}

		if(!scr.gamestart && scr.host.nround_ed && scr.host.nply_ed && !scr.gameover){
			// draw a retangle behind the text

			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "Setting Complete!";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (50), (81),
									  255, 0, 0);
			void* button = create_button((0), (81), (text_width + 50), (font_size+10), 0, 0, 0);
			draw_button(window, button);
			draw_text(window, title);
			delete_text(title);
			delete_button(button);
			const char* title_text2 = "press Start!";
			text_width = strlen(title_text2) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text2, font_size,
									  50, (81 + font_size + 10),
									  255, 0, 0);
			button = create_button((0), (81 + font_size + 10), (text_width + 50), (font_size+10), 0, 0, 0);
			draw_button(window, button);
			draw_text(window, title);
			delete_text(title);
			delete_button(button);
		}

		if(scr.gameover){
			// put "Game Over!" on the screen, when click the button, close the window
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "Game Over!";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  10, (600 - font_size) / 2,
									  255, 0, 0);
			void* button = create_button(10, (600 - font_size) / 2, (text_width + 50), (2 * font_size+30), 96, 10, 145);
			draw_button(window, button);
			draw_text(window, title);
			delete_text(title);
			title_text = "press Leave!";
			text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text, font_size,
									  10, (600 - font_size) / 2 + 50,
									  255, 0, 0);
			draw_text(window, title);
			delete_text(title);
			delete_button(button);
			if(has_focus(window) && is_mouse_button_pressed() && !ms_ps){
				ms_ps = true;
			}
			if (is_mouse_button_pressed() == false && ms_ps) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);
				if (is_button_clicked(window, button, mouse_x, mouse_y)) {
					close_window(window);
					return;
				}
			}

			char title_text3[50];
			const char* image_path;
			switch(scr.obs.wh){
				case 1:
					sprintf(title_text3, "Last befor end: gems");
					// put picture gem.jpg
					image_path = "gem.png"; // 圖片路徑
					break;
				case 2:
					sprintf(title_text3, "Last befor end: SNAKE");
					image_path = "snake.png"; // 圖片路徑
					break;
				case 3:
					sprintf(title_text3, "Last befor end: ROCKS");
					image_path = "rocks.png"; // 圖片路徑
					break;
				case 4:
					sprintf(title_text3, "Last befor end: FIRE");
					image_path = "fire.png"; // 圖片路徑
					break;
				case 5:
					sprintf(title_text3, "Last befor end: SPIDERS");
					image_path = "spider.png"; // 圖片路徑
					break;
				case 6:
					sprintf(title_text3, "Last befor end: ZOMBIES");
					image_path = "zombie.png"; // 圖片路徑
					break;
				case 7:
					sprintf(title_text3, "Last befor end: Treasure");
					char temp[256];
					sprintf(temp, "%d_point.jpg", scr.obs.num);
					image_path = temp; // 圖片路徑
					break;
				default:
					break;
			}

			void* image = load_image(image_path);
			if (!image) {
				errormsg("Failed to load image.\n");
			}
			draw_image(window, image, 300, 200);
			delete_image(image);

			text_width = (strlen(title_text3)-22) * font_size / 2; // 簡單估算文字寬度
			title = create_text(window, font_path, title_text3, font_size,
								(800 - text_width) / 2 - 150, 150,
								255, 20, 25);
			if (!title) {
				errormsg("Failed to create title text.\n");
			}
			draw_text(window, title);
			delete_text(title);
		}

		if(scr.room_ed){
			// draw a chat room at right of the screen
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "Chat Room";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  610, 81,
									  0, 0, 0);
			void* button = create_button(600, (81), (text_width + 50), 450, 191, 242, 229);
			draw_button(window, button);
			draw_text(window, title);
			delete_text(title);
			delete_button(button);

			// Render messages with word wrap
			int max_chars_per_line = 20; // Adjust based on window size
			char lines[10][256]; // Assuming max 10 lines per message
			int line_count = 0;

			int cnt = 0;

			for (int i = 0; i < 8; i++) {
				if (scr.msgs[i].str[0] != '\0') {
					split_text_into_lines(scr.msgs[i].str, max_chars_per_line, lines, &line_count);
					for (int j = 0; j < line_count; j++) {
						void* text = create_text(window, font_path, lines[j], 20,
												605, 150 + 30 * cnt, 0, 0, 255);
						draw_text(window, text);
						delete_text(text);
						cnt++;
					}
				}
			}


			// 創建並繪製輸入框
			button = create_button(190, 490, 390, 50, 255, 255, 255); // 白色background
			draw_button(window, button);
			delete_button(button);
			button = create_button(195, 495, 380, 40, 0, 0, 0); // 黑色邊框
			draw_button(window, button);
			delete_button(button);


			void* input_text = create_text(window, font_path, input_buffer, 30, 200, 495, 255, 255, 255);
			draw_text(window, input_text);
			delete_text(input_text);

			// 處理輸入事件
			if (has_focus(window)) {
				poll_events(window);
				char c;
				if(is_key_pressed() == true && key_ps == false){
					key_ps = true;
					c = get_pressed_key();
				}
				if (is_key_pressed() == false && key_ps == true) {
					key_ps = false;
					if (c == '\b' && input_len > 0) {  // 處理刪除鍵
						input_buffer[--input_len] = '\0';
					} 
					else if (c != '\b' && input_len < sizeof(input_buffer) - 1 && c >= 32 && c <= 126) {  // 處理可見字符
						input_buffer[input_len++] = c;
						input_buffer[input_len] = '\0';
					} else if(c == 13 && input_len > 0){
						// printf("Enter pressed\n");
						sprintf(sendline, "msg: %s", input_buffer);
						Writen(sockfd, sendline, strlen(sendline));
						// printf("msg sent: %s\n", scr.name);
						bzero(input_buffer, sizeof(input_buffer));
						input_len = 0;

					} 
					else {
						int nn = c;
						// printf("Invalid input: %d\n", nn);
					}
					
				}
			}
			
			
		}

        display_window(window);  // 顯示內容



        usleep(16000); // 模擬 60 FPS

		
    }


};

int main(int argc, char **argv){

	int					sockfd;
	struct sockaddr_in	servaddr;

	if (argc != 2)
		err_quit("usage: tcpcli <IPaddress> <ID>");

	sockfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT+5);
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
	printf("Connected to server successfully!\n");
	printf("\n");
	clr_scr();		// clear screen
	

	// memset
	memset(scr.name, 0, sizeof(scr.name));

	scr.host.ishost = false;
	scr.host.ishosted = false;
	

	scr.room_num = 0;

	// 創建 SFML 視窗
    void* window = create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Client Window");
    if (!window) {
        errormsg("Failed to create window.\n");
    }

    // 加載圖片
    const char* image_path = "Title.png"; // 圖片路徑
    void* image = load_image(image_path);
    if (!image) {
        errormsg("Failed to load image");
    }

	clear_window(window, 0, 0, 0);  // 黑色背景
	draw_image(window, image, (WINDOW_WIDTH - 700) / 2, 50); // 圖片放置於中央偏上
	
	display_window(window);  // 顯示內容

	printf("\n");
	clr_scr();		// clear screen
	printf("\n");

	xchg_data(stdin, sockfd, window, image);		/* do it all */
	
    // window is open, close it
	// delete_image(image);
	// close_window(window);
	shutdown(sockfd, SHUT_WR);

	return 0;
	
}
