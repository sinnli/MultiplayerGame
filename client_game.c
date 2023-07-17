//
// Created by liel on 06/01/2022.
//

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

char recv_buff[256];
int server_sock;
int game_is_on =1;
char expected_msg = '0';
int input_of_user;
char multiadd[18];
int udp_sock;
int quit = 0;
int race_is_on=0;

void get_message(char i);

void *mult_rcv_thread(void *);

void quit_game();

void main(int argc, char *argv[]) {
    //argv: 1-ip_of_server 2-port of server (TCP)
    char msg[256];
    int first = 0,opt=1;

    //UDP thread
    pthread_t  udp_tid;
    int rc = pthread_create(&udp_tid, NULL, &mult_rcv_thread, NULL);
    if (rc != 0){
        printf("\n ERROR:pthread_create  of udp thread failed \n");
        exit(1);
    }

    //TCP socket
    struct sockaddr_in serv_addr;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Failing of opening of client socket.\n");
        exit(1);
    }

    //set welcome socket options
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)))
    {
        perror("setsockopt\n");
        exit(EXIT_FAILURE);
    }
    //ipv4
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    //try to connect to server for a game
    if (connect(server_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        perror("connection to server failed.\n");
        exit(1);
    }
    else {
        printf("connection estabalished!\n");
        fd_set readfds;
        while (1) {
            //clear
            FD_ZERO(&readfds);
            //set server socket
            FD_SET(server_sock, &readfds);
            //set keyboard socket
            FD_SET(0, &readfds);

            int activity;
            activity = select(server_sock + 1, &readfds, NULL, NULL, NULL);
            if (activity < 0) { //check for error
                perror("select erorrr\n");
            }
            int ret;
            if (FD_ISSET(server_sock, &readfds)) { //check if server sent a message
                ret = recv(server_sock, (char *) recv_buff, sizeof(recv_buff), 0);
                if (ret < 0) {
                    perror("error in receiving data from server\n");
                    exit(1);
                } else if (ret >= 0) {
                    get_message(recv_buff[0]);
                }
            }
            else if(FD_ISSET(0, &readfds)) { //keyboard detected
                if (race_is_on == 1) {
                    char buff[255];
                    //getchar();
                    fflush(stdin);  //clear input buffer
                    fgets(buff, sizeof(buff), stdin);
                    //quit
                    if (buff[0] == 'q' || buff[0] == 'Q') {
                        memset(msg, '\0', 256);
                        msg[0] = '3';
                        send(server_sock, msg, sizeof(msg), 0);
                    }
                        //double money
                    else if (buff[0] == 'd' || buff[0] == 'D') {
                        memset(msg, '\0', 256);
                        msg[0] = '4';
                        send(server_sock, msg, sizeof(msg), 0);
                    }
                        //data request
                    else if (buff[0] == 'r' || buff[0] == 'R') {
                        memset(msg, '\0', 256);
                        msg[0] = '5';
                        send(server_sock, msg, sizeof(msg), 0);
                    }
                        //input not valid
                    else {
                        if (first == 1) {
                            printf("Sorry, we couldn't understand you:\n");
                            printf("for double money please enter 'd'.\n");
                            printf("for quit please enter 'q'.\n");
                            printf("for data request enter 'r'.\n");
                        }
                        first = 1;
                    }
                }
                else {
                    int c;
                    while ((c = getchar()) != '\n' && c != EOF);
                }
            }
        }
    }
}

void quit_game(){
    game_is_on = 0;
    race_is_on=0;
    close(udp_sock);
    close(server_sock);
    exit(1);
}

void *mult_rcv_thread(void *arg) {
    int i,j;
    int num_of_horse,opt = 1;
    char message[256];
    struct ip_mreq mreq;
    struct sockaddr_in multi_addr;
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    int addrlen = sizeof(multi_addr);

    multi_addr.sin_family = AF_INET;
    multi_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    multi_addr.sin_port = htons(2000);

    if(setsockopt(udp_sock,SOL_SOCKET,SO_REUSEPORT|SO_REUSEADDR,&opt,sizeof(opt))==-1){
        perror("setsock option of udp socket\n");
    }

    bind(udp_sock, (struct sockaddr *) &multi_addr, sizeof(multi_addr));

    mreq.imr_multiaddr.s_addr = inet_addr("230.1.1.234");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(udp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    while (game_is_on){
        //receiving racce state
        memset(message,'\0', 256);
        if(recvfrom(udp_sock, message, sizeof(message), 0, (struct sockaddr *) &multi_addr, &addrlen)==-1){
            perror("receiving through udp error!!\n");
        }
        //printing state of race to user
        printf("\n");
        num_of_horse = message[0];
        for (i = 0 ; i< num_of_horse-'0' ;i++){
            printf("horse #%d",i+1);
            for(j=0 ; j < 20 ; j++){
                if(j == (message[i+1]-'0')) printf("*");
                else printf(" ");
            }
            printf("|\n");
        }
        printf("\n");

        if(quit==1){
            setsockopt(udp_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
            printf("Dear Gambler,quitting is still in progress.\n"
                   "Please wait\n");
            break;
        }
    }
    //printf("\n");
    pthread_exit(NULL);
}

void get_message(char type) {
    int num_of_horses,j;
    char msg[256];
    switch ((int)type -'0') {
        case 0:
            if(expected_msg=='0'){
                race_is_on=1;
                printf("\nWelcome to Horse Gambel \nWhile the race is on you may do as follow:\n"
                       "In order to double money please enter 'd'.\n"
                       "In order to quit please enter 'q'.\n"
                       "In order to request data of the race please enter 'r'.\n"
                       "ENJOY the RIDE\n\n");
                memcpy(multiadd,recv_buff+1,strlen(recv_buff)-1);
                memset(msg,'\0',256);
                msg[0] = '0';
                send(server_sock,msg,sizeof(msg),0);
                expected_msg ='1';
            }
            else {//bad server or connection
                printf("expected to get msg 0 but got msg %c\n",expected_msg);
                quit_game();
            }
            break;
        case 1:
            if(expected_msg=='1'){
                //choose hourse msg
                printf("Choose a horse between 1 and %d:\n"
                       "*Warning: If incorrect input will be entered you will disconnected.*\n"
                       "",*((int*)(recv_buff+1)));
                fflush(stdin);
                scanf("%d", &input_of_user);
                memset(msg,'\0',256);
                msg[0] = '1';
                memcpy(msg+1,(char*)&input_of_user,sizeof(int));
                send(server_sock,msg,sizeof(msg),0);
                fflush(stdin); //clear input buffer
                expected_msg='2';
                break;
            }
            else {//bad server or connection
                printf("expected to get msg 1 but got msg %c\n",expected_msg);
                quit_game();
            }
            break;
        case 2:
            if(expected_msg=='2'){
                //amount of money
                memset(msg,'\0',256);
                while(getchar() != '\n'); //clear input buffer
                printf("How much money do you want to bet on?\n"
                       "*Warning: If incorrect input will be entered you will be disconnected.*\n");
                scanf("%d",&input_of_user);
                msg[0] = '2';
                memcpy(msg+1,(char*)&input_of_user,sizeof (int));
                while(getchar() != '\n'); //clear input buffer
                send(server_sock,msg,sizeof(msg),0);
            }
            else {            //bad connention
                printf("expected to get msg 2 but got msg %c\n",expected_msg);
                quit_game();
            }
            break;
        case 3:
            //quit while game
            printf("The amount that is returned by quitting is :%d \n",*((int*)(recv_buff+1)));
            quit = 1;
            quit_game();
            break;
        case 4:
            //duplicate amount of gamble money
            if(!quit) {
                printf("You have doubled the amount you bet on successfully\n");
            }
            break;
        case 5:
            //data request
            if(!quit) {
                memcpy(&num_of_horses, (int *) (recv_buff + 1), 4);
                printf("The total number of horses that are in the race is: %d\n", num_of_horses);
                int temp;
                for (j = 0; j < num_of_horses; j++) {
                    memcpy(&temp, (int *) (recv_buff + 9 + 4 * j), 4);
                    printf("bet on horse #%d is: %d $ \n", j + 1, temp);
                }
                memcpy(&temp, (int *) (recv_buff + 5), 4);
                printf("Winner prize is: %d $ ,WISH YOU LUCK ;)\n", temp);
            }
            break;
        case 6:
            //winner
            game_is_on = 0;
            if(!quit) {
                printf("You WON :)\n The amount of money you receive : %d\n", *((int *) (recv_buff + 1)));
            }
            quit_game();
            break;
        case 7:
            //looser
            game_is_on = 0;
            if(!quit) {
                printf("You have lost. Maybe next time you will have more luck\n");
            }
            quit_game();
            break;
        case 8:
            //disconnect message
            printf("something went wrong, you've been disconnected by the server\n");
            quit_game();
            break;
    }
    return;
}

