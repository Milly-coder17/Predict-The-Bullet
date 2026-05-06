/*
 * Filename:
 *      client.c
 *
 * Description:
 *      This program is used in tandem with server.c demonstrating the use of sockets to create a TCP client.
 *
 * Compile Instructions:
 *      `gcc -o client client.c'
 *
 * Author:
 *      Raphael Garay
 *      rgaray@gbox.adnu.edu.ph
 *      Ateneo de Naga University
 *
 * Notes:
 *      This is a handout for CSMC312: Operating Systems
 *      First Sem S/Y 2024-2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

void die_with_error(char *error_msg){
    printf("%s", error_msg);
    exit(-1);
}

typedef struct{
    int health;
    int energy;
    int bullet;
} Player;

int recv_int(int sock, int *value){
    int total = 0;
    int bytes;

    while(total < sizeof(int)){
        bytes = recv(sock, ((char*)value) + total, sizeof(int) - total, 0);
        if(bytes <= 0) return -1;
        total += bytes;
    }
    return 0;
}

int send_int(int sock, int value){
    int total = 0;
    int bytes;
    while(total < (int)sizeof(int)){
        bytes = send(sock, ((char*)&value) + total, sizeof(int) - total, 0);
        if(bytes <= 0) return -1;
        total += bytes;
    }
    return 0;
}

static int pick_followup_move(Player *p2) {
    int valid = 0;
    int move = 0;
    while (!valid) {
        printf("\nChoose your follow-up move:\n");
        printf("1. Shoot\n");
        printf("2. Load\n");
        printf("3. Block\n");
        printf("4. Deflect\n");
        printf("6. Double Shot (Special)\n");
        printf("Enter your move: ");
        scanf("%d", &move);
        valid = 1;

        if (move == 5) {
            printf("\nCannot cancel again!\n\n");
            valid = 0;
        } else if (move == 1 && p2->bullet <= 0) {
            printf("\nYou have no bullets!\n\n");
            valid = 0;
        } else if (move == 3 && p2->energy < 1) {
            printf("\nNot enough energy to block.\n\n");
            valid = 0;
        } else if (move == 4 && p2->energy < 3) {
            printf("\nNot enough energy to deflect.\n\n");
            valid = 0;
        } else if (move == 6 && (p2->energy < 3 || p2->bullet < 2)) {
            printf("\nNot enough energy or bullets for Double Shot.\n\n");
            valid = 0;
        } else if (move < 1 || move > 6) {
            printf("\nInvalid choice!\n\n");
            valid = 0;
        }
    }
    return move;
}

int main(int argc, char *argv[]){
    int client_sock, port_no;
    struct sockaddr_in server_addr;
    struct hostent *server;

    if(argc < 3){
        printf("Usage: %s hostname port_no", argv[0]);
        exit(1);
    }

    printf("Client starting ...\n");

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(client_sock < 0){
        die_with_error("Error: socket() Failed.");
    }

    server = gethostbyname(argv[1]);

    if(server == NULL){
        die_with_error("Error: No such host.");
    }
    port_no = atoi(argv[2]);

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

    bcopy((char*)server->h_addr,
        (char*)&server_addr.sin_addr.s_addr,
        server->h_length);

    server_addr.sin_port = htons(port_no);

    if(connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        die_with_error("Error: connect() Failed.");

    printf("Connection successful!\n\n");

    int player1move;
    int player2move;

    Player player1;
    player1.health = 5;

    Player player2;
    player2.health = 5;
    player2.energy = 2;
    player2.bullet = 0;

    while(1){
        int valid = 0;
        while(!valid){
            printf("===========================================\n");
            printf("Your Status:\n");
            printf("Health: ");
            for(int i = 0; i < player2.health; i++) {
            printf("♥︎ ");
            }
            printf("\nBullets: ");
            for(int i = 0; i < player2.bullet; i++) {
            printf("⁍ ");
            }
            printf("\nEnergy: ");
            for(int i = 0; i < player2.energy; i++) {
            printf("⚡ ");
            }
            printf("\n\nPlayer 1 Status:\n");
            printf("Health: ");
            for(int i = 0; i < player1.health; i++) {
            printf("♥︎ ");
            }
            printf("\n===========================================\n");
            printf("\nPlayer 2 choose a move:\n");
            printf("1. Shoot\n");
            printf("2. Load\n");
            printf("3. Block\n");
            printf("4. Deflect\n");
            printf("5. Cancel Turn (Special)\n");
            printf("6. Double Shot (Special)\n");
            printf("Enter your move: ");
            scanf("%d", &player2move);
            valid = 1;

            if(player2move == 6 && (player2.energy < 3 || player2.bullet < 2)){
                printf("\nNot enough energy or bullets for Double Shot!\n\n");
                valid = 0;
            }

            if(player2move == 5 && player2.energy < 5){
                printf("\nNot enough energy!\n\n");
                valid = 0;
            }

            if(player2move == 1 && player2.bullet <= 0){
                printf("\nYou have no bullets! You must load first.\n\n");
                valid = 0;
            }

            if(player2move == 3 && player2.energy < 1){
                printf("\nNot enough energy!\n\n");
                valid = 0;
            }

            if(player2move == 4 && player2.energy < 3){
                printf("\nNot enough energy!\n\n");
                valid = 0;
            }

            if(player2move > 6 || player2move < 1){
                printf("\nInvalid choice!\n\n");
                valid = 0;
            }
        }

        // SEND MOVE
        send(client_sock, &player2move, sizeof(int), 0);
        printf("\nWaiting for Player 1...\n\n");
        if(recv_int(client_sock, &player1move) < 0){
            die_with_error("Error: recv() Failed.");
        }

        // RECEIVE CANCEL TYPE
        int cancel_type = 0;
        if(recv_int(client_sock, &cancel_type) < 0){
            die_with_error("Error: recv() Failed.");
        }

        printf("Round result:\n");

        if (cancel_type == 1) {
            // Player 1 cancelled Player 2's move, then picks a follow-up
            printf("Player 1 used Cancel! Your move was negated!\n");
            printf("Player 1 is choosing a follow-up...\n\n");
            int new_p1move;
            if(recv_int(client_sock, &new_p1move) < 0){
                die_with_error("Error: recv() Failed.");
            }
            player1move = new_p1move;
            player2move = 0;
            switch(player1move) {
                case 1:
                    printf("Player 1 shot! You were hit!\n\n");
                    player2.health--;
                    break;
                case 2:
                    printf("Player 1 loaded.\n\n");
                    break;
                case 3:
                    printf("Player 1 blocked.\n\n");
                    break;
                case 4:
                    printf("Player 1 deflected.\n\n");
                    break;
                case 6:
                    printf("Player 1 Double Shot! You take 2 damage!\n\n");
                    player2.health -= 2;
                    break;
            }
        } else if (cancel_type == 2) {
            // Player 2 cancelled Player 1's move, then picks a follow-up
            player2.energy -= 5;
            printf("Your Cancel worked! Player 1's move was negated!\n");
            int new_p2move = pick_followup_move(&player2);
            if(send_int(client_sock, new_p2move) < 0){
                die_with_error("Error: send() Failed.");
            }
            player2move = new_p2move;
            player1move = 0;
            switch(player2move) {
                case 1:
                    printf("You shot! Player 1 was hit!\n\n");
                    player2.bullet--;
                    player1.health--;
                    break;
                case 2:
                    printf("You loaded.\n\n");
                    if(player2.bullet < 6) player2.bullet++;
                    break;
                case 3:
                    printf("You blocked (no threat).\n\n");
                    player2.energy--;
                    break;
                case 4:
                    printf("You deflected (no threat).\n\n");
                    player2.energy -= 3;
                    break;
                case 6:
                    printf("Double Shot! Player 1 takes 2 damage!\n\n");
                    player2.energy -= 3;
                    player2.bullet -= 2;
                    player1.health -= 2;
                    break;
            }
        } else if (cancel_type == 3) {
            // Both cancelled, both pick follow-up moves
            player2.energy -= 5;
            printf("Both used Cancel! Pick your follow-up move:\n");
            int new_p2move = pick_followup_move(&player2);
            if(send_int(client_sock, new_p2move) < 0){
                die_with_error("Error: send() Failed.");
            }
            int new_p1move;
            if(recv_int(client_sock, &new_p1move) < 0){
                die_with_error("Error: recv() Failed.");
            }
            player2move = new_p2move;
            player1move = new_p1move;
            printf("Resolving second moves:\n");

            switch(player2move){
                case 1:
                    switch(player1move){
                        case 1:
                            printf("Draw!\n\n");
                            break;
                        case 2:
                            printf("Player 1 loads!\nYou shot!\nPlayer 1 lost 1 health!\n\n");
                            player2.bullet--;
                            player1.health--;
                            break;
                        case 3:
                            printf("Player 1 blocked your shot!\n\n");
                            player2.bullet--;
                            break;
                        case 4:
                            printf("Player 1 deflected your shot! You got hit!\n\n");
                            player2.bullet--;
                            player2.health--;
                            break;
                        case 6:
                            printf("Player 1 Double Shot cancelled your shot!\n\n");
                            player2.bullet--;
                            break;
                    }
                    break;
                case 2:
                    switch(player1move){
                        case 1:
                            printf("You loaded! Player 1 shoots! You lost 1 health!\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            player2.health--;
                            break;
                        case 2:
                            printf("Both loaded!\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                        case 3:
                            printf("You loaded! Player 1 blocked.\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                        case 4:
                            printf("You loaded! Player 1 deflected.\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                        case 6:
                            printf("You loaded! Player 1 Double Shot!\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                    }
                    break;
                case 3:
                    player2.energy--;
                    switch(player1move){
                        case 1:
                            printf("Player 1 shoots! You blocked!\n\n");
                            break;
                        case 2:
                            printf("Player 1 loads! You blocked.\n\n");
                            break;
                        case 3:
                            printf("Both blocked.\n\n");
                            break;
                        case 4:
                            printf("Player 1 deflected. You blocked.\n\n");
                            break;
                        case 6:
                            printf("Player 1 Double Shot! Your block stopped 1!\n\n");
                            break;
                    }
                    break;
                case 4:
                    player2.energy -= 3;
                    switch(player1move){
                        case 1:
                            printf("You deflected! Player 1 got hit!\n\n");
                            player1.health--;
                            break;
                        case 2:
                            printf("Player 1 loads! You deflected.\n\n");
                            break;
                        case 3:
                            printf("Player 1 blocked. You deflected.\n\n");
                            break;
                        case 4:
                            printf("Both deflected.\n\n");
                            break;
                        case 6:
                            printf("Player 1 Double Shot! 1 deflected!\n\n");
                            break;
                    }
                    break;
                case 6:
                    player2.energy -= 3;
                    player2.bullet -= 2;
                    switch(player1move){
                        case 1:
                            printf("Your Double Shot cancelled Player 1's shot! 2 DMG!\n\n");
                            player1.health -= 2;
                            break;
                        case 2:
                            printf("Double Shot! Player 1 lost 2 health!\n\n");
                            player1.health -= 2;
                            break;
                        case 3:
                            printf("Player 1 blocked 1 bullet! 1 DMG!\n\n");
                            player1.health -= 1;
                            break;
                        case 4:
                            printf("1 deflected! Both took 1 DMG!\n\n");
                            player1.health -= 1;
                            player2.health -= 1;
                            break;
                        case 6:
                            printf("Collision! Both Double Shots cancelled!\n\n");
                            break;
                    }
                    break;
            }
            if(player1move == 6 && player2move != 6) {
                if(player2move == 1)      player2.health -= 2;
                else if(player2move == 2) player2.health -= 2;
                else if(player2move == 3) player2.health -= 1;
                else if(player2move == 4) { player2.health -= 1; player1.health -= 1; }
            }
        } else {
            // Normal round (cancel_type == 0)
            switch(player2move){
                case 1:
                    switch(player1move){
                        case 1:
                            printf("Draw!\n\n");
                            break;
                        case 2:
                            printf("Player 1 loads!\n");
                            printf("You shot!\n");
                            printf("Player 1 lost 1 health!\n\n");
                            player2.bullet--;
                            player1.health--;
                            break;
                        case 3:
                            printf("Player 1 blocked your shot!\n\n");
                            player2.bullet--;
                            break;
                        case 4:
                            printf("Player 1 deflected your shot!\n");
                            printf("You got hit!\n\n");
                            player2.bullet--;
                            player2.health--;
                            break;
                        case 6:
                            printf("Player 1 used Double Shot! Your shot was cancelled!\n\n");
                            player2.bullet--;
                            break;
                    }
                    break;
                case 2:
                    switch(player1move){
                        case 1:
                            printf("You loaded!\n");
                            printf("Player 1 shoots!\n");
                            printf("You lost 1 health!\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            player2.health--;
                            break;
                        case 2:
                            printf("You loaded!\n");
                            printf("Player 1 loads!\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                        case 3:
                            printf("You loaded!\n");
                            printf("Player 1 blocked, nothing happened.\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                        case 4:
                            printf("You loaded!\n");
                            printf("Player 1 deflected, nothing happened.\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                        case 6:
                            printf("You loaded!\n");
                            printf("Player 1 used Double Shot!\n\n");
                            if(player2.bullet < 6) player2.bullet++;
                            break;
                    }
                    break;
                case 3:
                    player2.energy--;
                    switch(player1move){
                        case 1:
                            printf("Player 1 shoots!\n");
                            printf("You blocked the shot!\n\n");
                            break;
                        case 2:
                            printf("Player 1 loads!\n");
                            printf("You blocked, nothing happened.\n\n");
                            break;
                        case 3:
                            printf("Player 1 blocked, nothing happened.\n");
                            printf("You blocked, nothing happened.\n\n");
                            break;
                        case 4:
                            printf("Player 1 deflected, nothing happened.\n");
                            printf("You blocked, nothing happened.\n\n");
                            break;
                        case 6:
                            printf("Player 1 used Double Shot! Your block stopped 1 bullet!\n\n");
                            break;
                    }
                    break;
                case 4:
                    player2.energy -= 3;
                    switch(player1move){
                        case 1:
                            printf("You deflected the shot!\n");
                            printf("Player 1 got hit!\n\n");
                            player1.health--;
                            break;
                        case 2:
                            printf("Player 1 loads!\n");
                            printf("You deflected, nothing happened.\n\n");
                            break;
                        case 3:
                            printf("Player 1 blocked, nothing happened.\n");
                            printf("You deflected, nothing happened.\n\n");
                            break;
                        case 4:
                            printf("Player 1 deflected, nothing happened.\n");
                            printf("You deflected, nothing happened.\n\n");
                            break;
                        case 6:
                            printf("Player 1 used Double Shot! 1 deflected!\n\n");
                            break;
                    }
                    break;
                case 6:
                    player2.energy -= 3;
                    player2.bullet -= 2;
                    switch(player1move){
                        case 1:
                            printf("Your Double Shot cancelled Player 1's shot! 2 DMG!\n\n");
                            player1.health -= 2;
                            break;
                        case 2:
                            printf("Double Shot hit! Player 1 lost 2 health!\n\n");
                            player1.health -= 2;
                            break;
                        case 3:
                            printf("Player 1 blocked 1 bullet! 1 DMG dealt!\n\n");
                            player1.health -= 1;
                            break;
                        case 4:
                            printf("1 deflected! Both took 1 DMG!\n\n");
                            player1.health -= 1;
                            player2.health -= 1;
                            break;
                        case 6:
                            printf("Collision! Both Double Shots cancelled!\n\n");
                            break;
                    }
                    break;
            }

            if(player1move == 6 && player2move != 5 && player2move != 6) {
                if(player2move == 1)      player2.health -= 2;
                else if(player2move == 2) player2.health -= 2;
                else if(player2move == 3) player2.health -= 1;
                else if(player2move == 4) { player2.health -= 1; player1.health -= 1; }
            }
        }

        if(player2.energy < 6) {
            player2.energy++;
        }

        int round_done;
        if(recv_int(client_sock, &round_done) < 0){
            die_with_error("Error: recv() Failed.");
        }
        else if(round_done == 2){
            printf("\nGame Over\n");
            break;
        }


    }

    close(client_sock);

    return 0;
}
