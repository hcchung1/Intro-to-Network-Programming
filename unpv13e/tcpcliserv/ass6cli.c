#include	"unp.h"
#include  <string.h>
#include  <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

char id[MAXLINE];

#define TIMEOUT_SEC 20

extern void* create_window(int width, int height, const char* title);
extern void clear_window(void* window, int r, int g, int b);
extern void display_window(void* window);
extern void close_window(void* window);
extern void* create_text(void* window, const char* font_path, const char* text, int size, int x, int y, int r, int g, int b);
extern void draw_text(void* window, void* text);
extern void delete_text(void* text);
extern void poll_events(void* window);

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

void xchg_data(FILE *fp, int sockfd, void* window, void* title)
{
    int       maxfdp1, stdineof, peer_exit, n;
    fd_set    rset;
    char      sendline[MAXLINE], recvline[MAXLINE];
	struct timeval timeout;
	timeout.tv_sec = TIMEOUT_SEC;
	timeout.tv_usec = 0;
	int stage = 0;
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
	clear_window(window, 0, 0, 0); // 黑色背景
	draw_text(window, title);      // 繪製標題
	display_window(window);        // 顯示內容

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
        Select(maxfdp1, &rset, NULL, NULL, NULL); // timeout for 20 seconds
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
				if(strcmp(recvline, "Enter your name: \n") == 0){
					printf("Enter your name: \n");
					/* 
					 * SFML
					 */
				} else if(strcmp(recvline, "Enter room number (1-5):\n") == 0){
					
					printf("Enter room number (1-5): \n");
					/* 
					 * SFML
					 */
					
				} else if(strcmp(recvline, "Sys: Make command now!\n") == 0){
					printf("Enter command: \n");
					/* 
					 * SFML
					 */
				} else if(stage == 5){
					if(strcmp(recvline, "Sys: Waiting for the result...\n") == 0){
						printf("Waiting for the result...\n");
						stage++;
					}
				} else {
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
				printf("sent: %s\n", sendline);
				bzero(sendline, MAXLINE);
				bzero(recvline, MAXLINE);
			};
        }
		// 清除背景並繪製內容
        clear_window(window, 0, 0, 0); // 黑色背景
        draw_text(window, title);      // 繪製標題
        display_window(window);        // 顯示內容

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

	// 創建 SFML 視窗
    void* window = create_window(800, 600, "Client Window");
    if (!window) {
        printf("Failed to create SFML window.\n");
        return -1;
    }

    // 創建標題，並放置在視窗正中間
    const char* font_path = "Arial.ttf"; // 字體路徑
    const char* title_text = "INCAN GOLD";
    int font_size = 30;
    int text_width = strlen(title_text) * font_size / 2; // 簡單估算文字寬度
    void* title = create_text(window, font_path, title_text, font_size,
                              (800 - text_width) / 2, (font_size) / 2,
                              255, 255, 255);
    if (!title) {
        printf("Failed to create title text.\n");
        close_window(window);
        return -1;
    }

	xchg_data(stdin, sockfd, window, title);		/* do it all */

	exit(0);
}