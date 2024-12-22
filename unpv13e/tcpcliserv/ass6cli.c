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

	
	
	// we need to know which of room are full, which are not.
	// using a while loop until user get into a room.
	// because a user may need a password to enter a room.
	
	set_scr();
	clr_scr();
	bzero(sendline, MAXLINE);
	bzero(recvline, MAXLINE);

	// end of giving ID to the server
	while (recvline[0] != 'a') {
		sprintf(sendline, "666 %s\n", id);
		sendline[strlen(sendline)] = '\0';	
		Writen(sockfd, sendline, strlen(sendline));
		printf("sent: %s", sendline);
		readline(sockfd, recvline, MAXLINE);
		printf("recv: %s", recvline);
		if(recvline[0] == 'a') {
			break;
		} else {
			printf("ID is not valid. Please enter again: ");
			Fgets(id, MAXLINE, fp);
			id[strlen(id)-1] = '\0';
		}
	}

	// 主遊戲迴圈
    // while (1) {
        
    //     Readline(sockfd, recvline, MAXLINE); // 接收伺服器訊息

    //     printf("Server: %s\n", recvline);

    //     // 處理伺服器指令
    //     if (strcmp(recvline, "Sys: GAME START") == 0) {
    //         printf("The game has started!\n");
    //     } else if (strcmp(recvline, "Sys: YOUR TURN") == 0) {
    //         printf("It's your turn! Enter a command (e.g., MOVE, ATTACK, EXIT): ");
    //         char command[50];
	// 		Fgets(command, 50, fp);
    //         command[strcspn(command, "\n")] = '\0'; // 移除換行符

    //         // 發送指令給伺服器
    //         sprintf(sendline, "%s\n", command);
	// 		Writen(sockfd, sendline, strlen(sendline));

    //         if (strcmp(command, "EXIT") == 0) {
    //             printf("Exiting the game...\n");
    //             break;
    //         }
    //     }
    // }
	


    for ( ; ; ) {	
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
				n = strlen(sendline);
				sendline[n] = '\n';
				Writen(sockfd, sendline, n+1);
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