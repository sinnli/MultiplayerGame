//
// Created by liel on 06/01/2022.
//
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#define max_num_clients 100
#define number_of_horses 6
#define new_max(x,y) (((x) >= (y)) ? (x):(y))

struct gambler{
    int amount;
    int num_of_horse;
    int id_of_client;
    int quit; // 0 means still in  the game
};
struct gambler *gamblers;

//variables

int *client_socket;
char multicast_addr[50]; //mutlicast address server gets from user
int clients_count;
int win_los_flag;
int moneyHorse[6] = {0}; //in size of num of horses
pthread_mutex_t gamblers_mutex;
pthread_mutex_t clients_mutex;
int num_of_hourses  = 6;
int amount_to_return;
int amount_money_winner;
int raceOn = 0;
int multicast_sock;
int winninghorse;
int endFlag=0;

void start_game();

void *client_initiate(void *);

void *client_TCP_thread(void *);

void *multicast_thread(void *);

void disconnect_client(int index);

int main(int argc, char *argv[]) {
    //argv: 1-muliticast address 2-TCP port

    strcpy(multicast_addr, argv[1]);

    //local main variables
    int i;
    struct timeval tvalBefore, tvalAfter;
    double Entrance_Timeout = 15;
    int max_socket;
    fd_set readfds;

    //mutex init
    pthread_mutex_init(&gamblers_mutex,NULL);
    pthread_mutex_init(&clients_mutex,NULL);

    //welcome socket
    struct sockaddr_in server_addr, client_addr;
    int welcome_sock,opt=1;

    //socket
    welcome_sock = socket(AF_INET, SOCK_STREAM, 0);//socket tcp
    if (welcome_sock == -1) {
        perror("error in creating server socket!\n");
        return 1;
    }

    //set welcome socket options
    if (setsockopt(welcome_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)))
    {
        perror("setsockopt\n");
        exit(EXIT_FAILURE);
    }

    //set server properties:
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[2]));


    //bind
    if (bind(welcome_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        perror("Error in bind of server.\n");
        return 1;
    }

    //listen
    listen(welcome_sock, max_num_clients);
    //defining array of sockets of clients
    client_socket = (int *) malloc(sizeof(int) * max_num_clients);
    for (i = 0; i < max_num_clients; i++) {
        client_socket[i] = -1;
    }

    int addrlen = sizeof(client_addr);
    int new_socket;
    int activity = 0;
    gettimeofday(&tvalBefore, NULL); //start time

    //server is always on
    while (1) {
        //timeout for waiting for clients
        printf("waiting for new clients...\n");
        gettimeofday(&tvalAfter, NULL); //current time
        while ((tvalAfter.tv_sec - tvalBefore.tv_sec) < Entrance_Timeout) {
            gettimeofday(&tvalAfter, NULL);

            //clear
            FD_ZERO(&readfds);
            //set server socket
            FD_SET(welcome_sock, &readfds);

            //select
            struct timeval timeout = {3, 0};
            activity = select(welcome_sock + 1, &readfds, NULL, NULL, &timeout);
            if (activity < 0) perror("error in select\n");
            if (activity == 0) continue; //time is over

            //if new client joined
            else if (FD_ISSET(welcome_sock, &readfds)) {
                new_socket = accept(welcome_sock, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
                if (new_socket < 0) {
                    perror("accepting new client failed.\n");
                    exit(1);
                }
                printf("new client joined\n");
                client_socket[clients_count] = new_socket;
                if(client_socket[clients_count]>max_socket){
                    max_socket = client_socket[clients_count];
                }
                clients_count++;
            }
        }//while watinig for clients for game

        printf("The number of clients after done timeout waiting is: %d\n",clients_count);
        if (clients_count >= 2) {  //more than 2 clients
            //start game
            start_game();
            printf("back for next round\n");

            //restart all relevant variables
            raceOn = 0;
            clients_count = 0;
            for (i = 0; i < max_num_clients; i++) {
                client_socket[i] = -1;
            }
            for(i=0;i<number_of_horses;i++){
                moneyHorse[i]=0;
            }
        }
        else printf("not enough gamblers, needs at least 2\n");
        gettimeofday(&tvalBefore, NULL); //current time
        gettimeofday(&tvalAfter, NULL); //current time

    }//while(1)

}

void start_game() {
    int j,rc;
    win_los_flag = 0;
    gamblers = (struct gambler*) malloc(sizeof(struct gambler)*clients_count);
    pthread_t  *thread_id = (pthread_t *) malloc(sizeof (pthread_t)*clients_count);
    pthread_t  *thread_id2 = (pthread_t *) malloc(sizeof (pthread_t)*clients_count);

    //create for each gambler initiate thread
    for (j = 0; j < clients_count; j++) {
        pthread_mutex_lock(&gamblers_mutex);
        gamblers[j].id_of_client = client_socket[j];
        pthread_mutex_unlock(&gamblers_mutex);
        int* parg = (int*) malloc(sizeof(j));
        *parg = j;
        rc = pthread_create(&thread_id[j], NULL, &client_initiate, parg);
        if (rc != 0){
            printf("\n ERROR:pthread_create failed \n");
            exit(1);
        }
    }//end initiate threads

    //wait for all the initiate thread to be done
    for (j = 0; j < clients_count; j++) {
        pthread_join(thread_id[j],NULL);
    }
    free(thread_id);

    raceOn = 1;

    //multicast thread
    if(clients_count!=0){   //may happen if all clients gave incorrect answers
        pthread_t multicast_id;
        rc = pthread_create(&multicast_id, NULL, &multicast_thread, NULL);
        if (rc != 0){
            printf("\n ERROR:pthread_create failed \n");
            exit(1);
        }
    }


    //now start all clients threads again for TCP connection
    for (j = 0; j < clients_count; j++) {
        int* parg = (int*) malloc(sizeof(j));
        *parg = j;
        while (!pthread_mutex_trylock(&clients_mutex));
        if(client_socket[j]!=-1){                //open a thread only if the client didnt quit already
            rc = pthread_create(&thread_id2[j], NULL, &client_TCP_thread, parg);
            if (rc != 0){
                printf("\n ERROR:pthread_create failed \n");
                exit(1);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    //wait for TCP threads
    for (j = 0; j < clients_count; j++) {
        pthread_join(thread_id2[j],NULL);
    }
    free(thread_id2);
    for(j=0;j<num_of_hourses;j++){
        moneyHorse[j] = 0;
    }
    raceOn = 0;
}

void *multicast_thread(void *arg) {
    int ttl = 64;
    raceOn = 1;
    int j, horse, quited;
    int most_adva_horse;
    char horse_to_send[256];
    memset(horse_to_send, '\0', 256);
    memset(horse_to_send, '0', number_of_horses + 1);
    horse_to_send[0] = number_of_horses + '0';

    struct sockaddr_in multi_addr;
    multicast_sock = socket(AF_INET, SOCK_DGRAM, 0);//socket tcp
    if (multicast_sock == -1) {
        perror("error in creating server socket!");
        pthread_exit(NULL);
    }

    bzero(&multi_addr, sizeof(multi_addr));
    multi_addr.sin_family = AF_INET;
    multi_addr.sin_addr.s_addr = inet_addr("230.1.1.234"); //multicast addr(temporary changed to loopback 127.0.0.1)
    multi_addr.sin_port = htons(2000);

    // change here tll

    setsockopt(multicast_sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    setsockopt(multicast_sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    //send state of race each interval of time

    while (raceOn) {
        printf("racOn\n");
        sendto(multicast_sock, horse_to_send, sizeof(horse_to_send), 0, (struct sockaddr *) &multi_addr,
               sizeof(multi_addr));
        sleep(3);
        //advance each horse
        most_adva_horse = rand() % (num_of_hourses + 1 - 1) + 1; //random horse is most advanced
        for (j = 1; j < num_of_hourses + 1; j++) {
            if (horse_to_send[j] == 20 + '0') {
                //found winner
                winninghorse = j;
                raceOn = 0;
                endFlag = 1;
                break;
            }
            if (j == most_adva_horse) {
                //current most advanced horse
                horse_to_send[j] += 2;
            } else {
                //the rest of the horses are slower
                horse_to_send[j] += 1;
            }
        }
    }
    if (endFlag == 1) {
        //winning procedure
        int num_of_winners = 0;
        int total_money = 0;
        while (!pthread_mutex_trylock(&gamblers_mutex));
        for (j = 0; j < clients_count; j++) {
            horse = gamblers[j].num_of_horse; //number of horse
            quited = gamblers[j].quit; //indicates that client quited race
            if (horse == winninghorse && !quited) {
                num_of_winners++;
            }
            total_money += gamblers[j].amount;
        }
        num_of_winners = new_max(num_of_winners,1);
        pthread_mutex_unlock(&gamblers_mutex);
        amount_money_winner = total_money / num_of_winners;
        win_los_flag = 1;
        endFlag = 0;
        pthread_exit(NULL);
    }
}

void *client_TCP_thread(void *arg) {
    int index = *(int *) arg;
    int ret,j,quit = 0;
    int sock,activity;
    int horse;
    char recv_buff[256],msg[256];
    fd_set readfds;

    //acssecing socket
    while (!pthread_mutex_trylock(&clients_mutex));
    sock = client_socket[index];
    pthread_mutex_unlock(&clients_mutex);

    //if Q or D
    while(raceOn) {

        FD_SET(sock,&readfds);
        //select
        struct timeval timeout = {5, 0};
        activity = select(sock + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) perror("errror in select\n");
        if (activity == 0) continue; //time is over

        ret = recv(sock, (int *) recv_buff, sizeof(recv_buff), 0);
        if (ret < 0) {
            perror("error in receiving data from client\n");
            exit(1);
        } else {
            if (recv_buff[0] == '3') {
                //Quit
                while (!pthread_mutex_trylock(&gamblers_mutex));
                amount_to_return = gamblers[index].amount / 2;
                moneyHorse[gamblers[index].num_of_horse-1] -= amount_to_return;
                gamblers[index].quit = 1; //indicates that client quited race
                pthread_mutex_unlock(&gamblers_mutex);
                //quit while game
                quit = 1;
                memset(msg,'\0',256);
                msg[0] = '3';
                memcpy(msg+1,(char*)&amount_to_return, sizeof(int));
                send(sock, msg, 256, 0);
                disconnect_client(index);
                pthread_exit(NULL);

            } else if (recv_buff[0] == '4') {
                //Double amount
                memset(msg, '\0', 256);
                msg[0] = '4';
                //mutex
                while (!pthread_mutex_trylock(&gamblers_mutex));
                moneyHorse[gamblers[index].num_of_horse-1] += gamblers[index].amount;
                gamblers[index].amount = gamblers[index].amount * 2;
                pthread_mutex_unlock(&gamblers_mutex);
                send(sock, msg, 256, 0);

                //else not in game anymore
            } else if (recv_buff[0] == '5') {
                //Data about horses and amount of money in race
                int total = 0;
                memset(msg, '\0', 256);
                msg[0] = '5';
                memcpy(msg + 1, (char *) &num_of_hourses, sizeof(int));//msg[1-4]
                for (j = 0; j < num_of_hourses; j++) {
                    memcpy(msg + 9 + 4 * j, (char *) (moneyHorse + j), sizeof(int));
                    total += moneyHorse[j];
                }
                memcpy(msg + 5, (char *) &total, sizeof(int));//msg[5-8]
                send(sock, msg, 256, 0);
            }
        }
    }

    while(!win_los_flag); //wait to finish results calculations

    while (!pthread_mutex_trylock(&gamblers_mutex));
    horse = gamblers[index].num_of_horse; //number of hourse
    pthread_mutex_unlock(&gamblers_mutex);

    if (!quit && horse == winninghorse) {
        //winner
        memset(msg,'\0',256);
        msg[0] = '6';
        memcpy(msg+1,(char*)&amount_money_winner,sizeof(int)) ;
        send(sock, msg, 256 , 0);
    }
    else
    {
        //client lost
        memset(msg, '\0', 256);
        msg[0] = '7';
        send(sock, msg, 256, 0);
    }
    /*
    close(sock);
    free(arg);
    return NULL ;
     */
    disconnect_client(index);
}

void *client_initiate(void * arg) {
    int index = *(int*)arg;
    char msg[256];
    int ret,sock;
    char recv_buff[256],temp;
    int tempint;

    //acssecing socket
    while(!pthread_mutex_trylock(&clients_mutex));
    sock = client_socket[index];
    pthread_mutex_unlock(&clients_mutex);

    //question 0
    memset(msg,'\0',256);
    msg[0] = '0';
    memcpy(msg+1,multicast_addr, 18);
    if(send(sock,msg,sizeof (msg),0)==-1){
        printf("error in sending question\n");
        msg[0] = '8';
        send(sock, msg, sizeof (msg), 0);
        disconnect_client(index);
        pthread_exit(NULL);
    }
    ret = recv(sock, recv_buff, 256,0);
    if (ret < 0) {
        perror("error in receiving data from client\n");
        msg[0] = '8';
        send(sock, msg, sizeof (msg), 0);
        disconnect_client(index);
        pthread_exit(NULL);
    }
    else {
        if(recv_buff[0] =='0'){
            //ack was received
        }
        //bad client or bad connention
        else {
            printf("expected to get message from type 0, but did not\n");
            msg[0] = '8';
            send(sock, msg, sizeof (msg), 0);
            disconnect_client(index);
            pthread_exit(NULL);
        }
    }
    //question #1 : choose horse
    msg[0] = '1';
    memcpy(msg+1,(char*)&num_of_hourses, sizeof(int));
    send(sock, msg, sizeof (msg), 0);
    ret = recv(sock, (int *) recv_buff, sizeof(recv_buff), 0);
    if (ret < 0) {
        perror("error in recievig data from client\n");
        msg[0] = '8';
        send(sock, msg, sizeof (msg), 0);
        disconnect_client(index);
        pthread_exit(NULL);
    }
    else {
        if(recv_buff[0] =='1'){
            //check if got correct input
            memcpy(&tempint,(int*)(recv_buff+1),sizeof (int));
            if (tempint<(num_of_hourses+1)&& tempint>0) {
                while (!pthread_mutex_trylock(&gamblers_mutex));
                gamblers[index].num_of_horse = tempint; //needs to be send in int
                //printf("gambler #%d chose horse %d\n", gamblers[index].id_of_client -3, gamblers[index].num_of_horse);
                printf("gambler #%d chose horse %d\n", index+1, gamblers[index].num_of_horse);
                pthread_mutex_unlock(&gamblers_mutex);
            }
            else{
                // incorrect data from client
                msg[0] = '8';
                send(sock, msg, sizeof (msg), 0);
                disconnect_client(index);
                pthread_exit(NULL);
            }
        }
        else{
            printf("expected to get message from type 1, but did not\n");
            msg[0] = '8';
            send(sock, msg, sizeof (msg), 0);
            disconnect_client(index);
            pthread_exit(NULL);
        }
    }
    //question #2:  amount of money
    msg[0] = '2';
    send(sock, msg, sizeof(msg), 0);
    ret = recv(sock, recv_buff, sizeof(recv_buff), 0);
    if (ret < 0) {
        perror("error in receiving data from client\n");
        close(sock);
        free(arg);
        pthread_exit(NULL);
    }
    else {
        if (recv_buff[0] == '2') {
            memcpy(&tempint,(int*)(recv_buff+1),4);
            if(tempint>0) {
                while (!pthread_mutex_trylock(&gamblers_mutex));
                gamblers[index].quit = 0;
                gamblers[index].amount = tempint;//shoud be int before sending
                moneyHorse[gamblers[index].num_of_horse-1] += gamblers[index].amount;
                printf("gambler #%d bet on %d$\n", index+1, gamblers[index].amount);
               pthread_mutex_unlock(&gamblers_mutex);
            }
            else{
                // incorrect data from client
                msg[0] = '8';
                send(sock, msg, sizeof (msg), 0);
                disconnect_client(index);
                pthread_exit(NULL);
            }
        } else {   //bad client or bad connention
            close(sock);
            free(arg);
            pthread_exit(NULL);
        }
    }

    free(arg);
    pthread_exit(NULL);
}

void disconnect_client(int index) {
    while (!pthread_mutex_trylock(&gamblers_mutex));
    gamblers[index].quit = 1;
    gamblers[index].amount = 0;
    printf("due to invalid answer/quit message gambler#%d is disconnected\n",index+1);
    pthread_mutex_unlock(&gamblers_mutex);
    while(!pthread_mutex_trylock(&clients_mutex));
    client_socket[index] = -1;
   // close(client_socket[index]);
    pthread_mutex_unlock(&clients_mutex);
   // clients_count--;
}
