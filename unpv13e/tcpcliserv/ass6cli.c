#include	"unp.h"
#include  <string.h>
#include  <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

char id[MAXLINE];

#define TIMEOUT_SEC 20;

extern void* create_window(int width, int height, const char* title);
extern void clear_window(void* window, int r, int g, int b);
extern void display_window(void* window);
extern void close_window(void* window);
extern int is_window_open(void* window);
extern int poll_event(void* window);

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

void xchg_data(FILE *fp, int sockfd)
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

	
	
	// we need to know which of room are full, which are not.
	// using a while loop until user get into a room.
	// because a user may need a password to enter a room.
	
	set_scr();
	clr_scr();
	bzero(sendline, MAXLINE);
	bzero(recvline, MAXLINE);

	bool first = false;

    for ( ; ; ) {	
		if(!first) {printf("firsttime\n"); first = true;};
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
				printf("\x1B[0;36m%s\x1B[0m", recvline);
				fflush(stdout);
				// if(strcmp(recvline, "Enter your name: \n") == 0){
				// 	// send ID
				// 	sprintf(sendline, "%s\n", id);
				// 	Writen(sockfd, sendline, strlen(sendline));
				// 	printf("sent: %s", sendline);

				// } else if(strcmp(recvline, "Enter room number (1-5):\n") == 0){
				// 	// send room number
				// 	printf("Enter room number (1-5): ");
				// 	stage = 3;
				// }else if(strcmp(recvline, "Sys: Make command now!\n") == 0){
				// 	// make command
				// 	printf("Enter command: ");
				// 	stage = 2;
				// }
				if(stage == 0){
					if(strcmp(recvline, "Enter your name: \n") == 0){
						printf("Enter your name: ");
						stage++;
					}
				} else if(stage == 2){
					if(strcmp(recvline, "Enter room number (1-5):\n") == 0){
						printf("Enter room number (1-5): ");
						stage++;
					}
				} else if(stage == 4){
					if(strcmp(recvline, "Sys: Make command now!\n") == 0){
						printf("Enter command: ");
						stage++;
					}
				} else if(stage == 5){
					if(strcmp(recvline, "Sys: Waiting for the result...\n") == 0){
						printf("Waiting for the result...\n");
						stage++;
					}
				}
			}
			else { // n < 0
			    printf("(server down)");
				return;
			};
        }
		
        if (FD_ISSET(fileno(fp), &rset)) {  /* input is readable */

            if (Fgets(sendline, MAXLINE, fp) == NULL) {
				if (peer_exit)
					return;
				else {
					printf("(leaving...)\n");
					stdineof = 1;
					Shutdown(sockfd, SHUT_WR);      /* send FIN */
				};
            }
			else {
				if(stage == 1){
					// send ID
					sprintf(sendline, "User name: %s\n", id);
					printf("sent: %s", sendline);
					Writen(sockfd, sendline, strlen(sendline));
					stage++;
				}else if(stage == 3){
					// send room number
					printf("sent: %s", sendline);
					Writen(sockfd, sendline, strlen(sendline));
					stage++;
				} else if(stage == 5){
					// make command
					printf("sent: %s", sendline);
					Writen(sockfd, sendline, strlen(sendline));
					stage = 4;
				} else {
					printf("Please wait for the server to send you a message.\n");
				}
			};
        }
    }
};

int
main(int argc, char **argv)
{
	int					sockfd;
	struct sockaddr_in	servaddr;

	if (argc != 3)
		err_quit("usage: tcpcli <IPaddress> <ID>");

	sockfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT+5);
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
	strcpy(id, argv[2]);
	id[strlen(id)] = '\0';

	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));// three way handshake
	printf("Sys: connected to server!\n");

	xchg_data(stdin, sockfd);		/* do it all */

	// closing the connection before exit
	close(sockfd);
	exit(0);
}