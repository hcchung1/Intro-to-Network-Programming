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

int stage = 0;

typedef struct Screen {
	char name[128];
	bool name_ing;
	bool name_ed;
	bool room_ing;
	bool room_ed;
	bool host[3]; // [0] = ishost, [1] = player number, [2] = round
	int room_num; // decide which room to join
	int player_num; // decide how many players
	int round_num; // decide how many rounds
} screen;

screen scr;

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
        Select(maxfdp1, &rset, NULL, NULL, &timeout); // timeout for 20 seconds
		if (FD_ISSET(sockfd, &rset)) {  /* socket is readable */
			n = read(sockfd, recvline, MAXLINE);
			if (n == 0) {
 		   		if (stdineof == 1)
                    return;         /* normal termination */
		   		else {
					printf("(End of input from the peer!)");
					peer_exit = 1;
					return;
				};
            }
			else if (n > 0) {
				// change here to know everyting about the room
				recvline[n] = '\0';
				// printf("received: ");
				// printf("%s\n", recvline);
				// bzero(recvline, MAXLINE);
				// printf("%d\n", stage);
				// fflush(stdout);
				char cmp0[256];
				sprintf(cmp0, "Welcome, %s! Enter room number (1-5):\n", scr.name);
				cmp0[strlen(cmp0)] = '\0';

				char cmp1[256];
				sprintf(cmp1, "You are the host of room %d. Set the number of players (5-8):\n", scr.room_num);
				cmp1[strlen(cmp1)] = '\0';
				
				if(strcmp(recvline, "Enter your name:\n") == 0){
					printf("Enter your name: \n");
					scr.name_ing = true;

				} 
				else if(strcmp(recvline, cmp0) == 0){
					
					printf("Enter room number (1-5): \n");
					scr.room_ing = true;

				} 
				else if(strcmp(recvline, "Sys: Make command now!\n") == 0){
					printf("Enter command: \n");
				} 
				else if(strcmp(recvline, cmp1) == 0){
					printf("You are the host!!\n");
					printf("Pick the number of players now!\n");
					scr.host[0] = true;
				} 
				else {
					printf("%s\n", recvline);
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
				// printf("sent: %s\n", sendline);
				if(scr.name_ing){
					scr.name_ed = true;
					scr.name_ing = false;
					printf("name: %s\n", sendline);
					// store the name
					strcpy(scr.name, sendline);
				}
				bzero(sendline, MAXLINE);
				bzero(recvline, MAXLINE);
			};
        }

		
		// 清除背景並繪製內容
        clear_window(window, 0, 0, 0);  // 黑色背景
        draw_image(window, image, (WINDOW_WIDTH - 700) / 2, 50); // 圖片放置於中央偏上
		if(scr.name_ing){
			const char* font_path = "Arial.ttf"; // 字體路徑
			const char* title_text = "Enter your name in terminal";
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  (800 - text_width) / 2, (600 - font_size) / 2,
									  255, 255, 255);
			if (!title) {
				printf("Failed to create title text.\n");
				close_window(window);
				return;
			}
			draw_text(window, title);
			// // 繪製輸入框
			// draw_rectangle(window, (20 + text_width), (600-font_size)/2, 200, 50, 255, 255, 255);  // 白色矩形
			// draw_rectangle(window, (22 + text_width), (600-font_size)/2+2, 196, 46, 0, 0, 0);        // 黑色內部
			//  // 顯示輸入框的內容
			// void* input_text = create_text(window, font_path, input_buffer, 30, 60, 260, 255, 255, 255);
			// draw_text(window, input_text);
			// delete_text(input_text);
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
			if (!title) {
				printf("Failed to create title text.\n");
				close_window(window);
				return;
			}
			draw_text(window, title);
			delete_text(title);
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
				char num[2];
				sprintf(num, "%d", i + 1);
				void* num_text = create_text(window, font_path, num, 30, 200 + i * 100 + 15, 300 + 10, 255, 0, 255);
				draw_text(window, num_text);
				delete_text(num_text);
			}

			// 檢測按鈕點擊
			if(is_mouse_button_pressed() == true && ms_ps == false){
				ms_ps = true;
			}
			if (is_mouse_button_pressed() == false && ms_ps == true) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);

				for (int i = 0; i < 5; i++) {
					if (is_button_clicked(window, buttons[i], mouse_x, mouse_y)) {
						printf("Button %d clicked!\n", i + 1);
						sprintf(sendline, "%d", i + 1);
						sendline[strlen(sendline)] = '\0';
						scr.room_num = i + 1; // 記錄被點擊的按鈕編號
						Writen(sockfd, sendline, strlen(sendline));
						printf("sent: %s\n", sendline);
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
		if(scr.room_ed){
			// put name on left bottom
			const char* font_path = "Arial.ttf"; // 字體路徑
			// make title_text to be "Room: %d"
			char title_text[50];
			sprintf(title_text, "Room: %d", scr.room_num);
			int font_size = 30;
			int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
			void* title = create_text(window, font_path, title_text, font_size,
									  600, 600 - font_size*2,
									  255, 255, 255);
			if (!title) {
				printf("Failed to create title text.\n");
				close_window(window);
				return;
			}
			draw_text(window, title);
			delete_text(title);
		}
		if(scr.host[0] && scr.host[1] == false){
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
			if(is_mouse_button_pressed() && !ms_ps){
				ms_ps = true;
			}
			if (is_mouse_button_pressed() == false && ms_ps) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);

				for (int i = 0; i < 4; i++) {
					if (is_button_clicked(window, buttons[i], mouse_x, mouse_y)) {
						printf("Button %d clicked!\n", i + 5);
						sprintf(sendline, "%d", i + 5);
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
						scr.host[1] = true;
						scr.player_num = i + 5;
					}
				}
			}

			// 刪除按鈕資源
			for (int i = 0; i < 4; i++) {
				delete_button(buttons[i]);
			}
		} else if(scr.host[0] && scr.host[1] && scr.host[2] == false){
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
			if(is_mouse_button_pressed() && !ms_ps){
				ms_ps = true;
			}
			if (is_mouse_button_pressed() == false && ms_ps) {
				ms_ps = false;
				int mouse_x = get_mouse_position_x(window);
				int mouse_y = get_mouse_position_y(window);

				for (int i = 0; i < 3; i++) {
					if (is_button_clicked(window, buttons[i], mouse_x, mouse_y)) {
						printf("Button %d clicked!\n", i + 3);
						sprintf(sendline, "%d", i + 3);
						sendline[strlen(sendline)] = '\0';
						Writen(sockfd, sendline, strlen(sendline));
						printf("sent: %s\n", sendline);
						bzero(sendline, MAXLINE);
						scr.host[2] = true;
						scr.round_num = i + 3;
					}
				}
			}

			// 刪除按鈕資源
			for (int i = 0; i < 3; i++) {
				delete_button(buttons[i]);
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

	// memset
	memset(scr.name, 0, sizeof(scr.name));

	for(int i = 0; i < 3; i++){
		scr.host[i] = false;
	}

	scr.room_num = 0;

	// 創建 SFML 視窗
    void* window = create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Client Window");
    if (!window) {
        printf("Failed to create SFML window.\n");
        return -1;
    }

    // 加載圖片
    const char* image_path = "Title.png"; // 圖片路徑
    void* image = load_image(image_path);
    if (!image) {
        printf("Failed to load image: %s\n", image_path);
        close_window(window);
        return -1;
    }

	clear_window(window, 0, 0, 0);  // 黑色背景
	draw_image(window, image, (WINDOW_WIDTH - 700) / 2, 50); // 圖片放置於中央偏上
	display_window(window);  // 顯示內容

	xchg_data(stdin, sockfd, window, image);		/* do it all */
	delete_image(image);
    close_window(window);
	exit(0);
}
