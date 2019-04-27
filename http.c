/*
    Computer System - Project 1 (Image Tagging)
    Author: Jeremy Tee (856782)
    Purpose: Handles http request and functions that supports game logic (comparing keywords, check user state etc)
*/
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "http.h"

/*type of states 
0 - intro page
1 - start page
2 - first round start/discard/accept page
3 - endgame for first round page
4 - second round start/discard/accept page
5 - endgame for second round page
6 - gameover page*/

bool handle_http_request(int sockfd, int state[], char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD], 
    char user_cookie_mapping[][MAX_SIZE_OF_KEYWORD], int current_player_cookies[]){
    // try to read the request
    char buff[2049];
    bzero(buff, 2049);

    char res_buff[2049];
    int n = read(sockfd, buff, 2049);
    if (n <= 0)
    {
        if (n < 0) {
            perror("read");
        }
        return false;
    }

    // terminate the string
    buff[n] = 0;
    char * curr = buff;

    // parse the method
    METHOD method = UNKNOWN;
    if (strncmp(curr, "GET ", 4) == 0)
    {
        curr += 4;
        method = GET;
    }
    else if (strncmp(curr, "POST ", 5) == 0)
    {
        curr += 5;
        method = POST;
    }
    else if (write(sockfd, HTTP_400, HTTP_400_LENGTH) < 0)
    {
        perror("write");
        return false;
    }
    
    // sanitise the URI
    while (*curr == '.' || *curr == '/')
        ++curr;

    //for favicon handling purposes
    if(*curr == 'f'){
       if(write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0){
            perror("write");
            return false;
       }
    }

    //get cookie from header if available (0 <= cookie value < 10)
    int cookie = get_cookie(curr);

    // new player as no cookie was detected
    if(cookie < 0){
        if (method == GET){
            if(!write_header_send_file("1_intro.html", buff, HTTP_200_FORMAT, sockfd, n)){
                return false;
            }
        } else if(method == POST){
            // locate the username 
            char * username = strstr(buff, "user=") + 5;
            int username_length = strlen(username);
            
            //assign a unique cookie number for the user
            int cookieCounter = 0;
            while(cookieCounter < MAX_COOKIE){
                if(strlen(user_cookie_mapping[cookieCounter]) == 0){
                    break;
                }
                cookieCounter++;
            }

            //copy to user_cookie_mapping using strncpy to ensure that it will not be overwritten 
            strncpy(user_cookie_mapping[cookieCounter], username, username_length+1); 

            //include <p></p>
            long added_length = username_length +7;

            // get the size of the file     
            struct stat st;
            stat("2_start.html", &st);
            long size = st.st_size + added_length;
            n = sprintf(buff, HTTP_200_FORMAT_COOKIE, size, cookieCounter);
            
            // send the header first
            if (write(sockfd, buff, n) < 0){
                perror("write");
                return false;
            }
            
            // read the content of the HTML file
            int filefd = open("2_start.html", O_RDONLY);
            n = read(filefd, buff, 2048);
            if (n < 0) {
                perror("read");
                close(filefd);
                return false;
            }
            close(filefd);

            //find the split section to insert username
            char* split = strstr(buff, "<form method=\"GET\">");
            int split_length = strlen(split);
            int buffer_length = strlen(buff);

            //initialise response buffer to be 0
            bzero(res_buff, 2049);

            //copy html content until the split into the res buff
            strncpy(res_buff, buff, buffer_length - split_length);

            //add in the username into the res buff
            strcat(res_buff, "<p>");
            strcat(res_buff, user_cookie_mapping[cookieCounter]);
            strcat(res_buff, "</p>");

            //add in the other html after the split
            strcat(res_buff, split);
            if (write(sockfd, res_buff, size) < 0){
                perror("write");
                return false;
            }
            state[cookieCounter] = START_PAGE;

            //assign player 0/1
            int player_num = next_player_num(current_player_cookies);
            current_player_cookies[player_num] = cookieCounter;
        }

    // cookie is present in header
    } else {
        //check if user has added itself as a current player and assign them as either player 0/1
        register_player(cookie, current_player_cookies);

        //get player num for current user
        int current_player_num = get_player_num(cookie, current_player_cookies);

        //get player num for opponent
        int opponent_player_num = (current_player_num + 1) % 2;

        //get opponent cookie
        int opponent_cookie = get_opponent_cookie(current_player_cookies, cookie);

        //check if opponent has quit, if yes then gameover
        if(opponent_cookie >= 0 && state[opponent_cookie] == GAMEOVER){
            state[cookie] = GAMEOVER;
            if(!write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n)){
                    return false;
            }
            return false;
        
        //opponent still in game
        //case 1: user with cookie getting info_page should be redirected to start_page
        } else if(state[cookie] == INFO_PAGE){
            state[cookie] = START_PAGE;
            int filefd;
            struct stat st;
            long size;

            // get the size of the file
            stat("2_start.html", &st);
            filefd = open("2_start.html", O_RDONLY);
            size = st.st_size + strlen(user_cookie_mapping[cookie]) + 9;

            n = sprintf(buff, HTTP_200_FORMAT, size);

            // send the header first
            if (write(sockfd, buff, n) < 0){
                perror("write");
                return false;
            }

            // send the file
            bzero(buff, 2049);
            n = read(filefd, buff, 2048);
            if (n < 0){
                perror("read");
                close(filefd);
                return false;
            }
            close(filefd);

            //clear response buffer
            bzero(res_buff, 2048);
            int buffer_length = strlen(buff);

            //find the split section to insert username
            char* split = strstr(buff, "<form method=\"GET\">");
            int split_length = strlen(split);
            strncpy(res_buff, buff, buffer_length - split_length);
            strcat(res_buff, "<p>");
            strcat(res_buff, user_cookie_mapping[cookie]);
            strcat(res_buff, "</p>");

            //add in the other html after the split
            strcat(res_buff, split);

            if (write(sockfd, res_buff, size) < 0){
                perror("write");
                return false;
            }

        //case 2: user at start page
        } else if (state[cookie] == START_PAGE){
            if (method == GET){
                // get the size of the file
                if(!write_header_send_file("3_first_turn.html", buff, HTTP_200_FORMAT, sockfd, n)){
                    return false;
                }
                state[cookie] = FIRST_ROUND;

            } else if (method == POST){
                if(!write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n)){
                    return false;
                }
                state[cookie] = GAMEOVER;
                return false;
            }

        //case 3: user at first_round
        } else if (state[cookie] == FIRST_ROUND){
            // only post methods allowed in this page - either (quit or start with keyword)
            // check if post just for completeness
            if (method == POST){
                //logic: if keyword present in header implies user starts guessing, else implies quit
                char* keyword = strstr(buff, "keyword=");
                if (keyword){
                    //implies both players are ready
                    if(state[opponent_cookie] == FIRST_ROUND){
                        keyword = strtok(keyword + 8, "&");
                        int counter = next_guess_num(guesses, cookie);
                        //copy keyword into guesses
                        strncpy(guesses[current_player_num][counter], keyword, strlen(keyword));
                        guesses[current_player_num][counter][strlen(keyword)+1] = '\0';
                        
                        // check if user guessed the correct keyword of opponent
                        // if correct send endgame
                        if(validate_keyword(guesses[current_player_num][counter], guesses, opponent_player_num)){
                            if(!write_header_send_file("6_endgame.html", buff, HTTP_200_FORMAT, sockfd, n)){
                                return false;
                            }
                            state[cookie] = ENDGAME_ONE;

                        // keyword is an incorrect guess
                        } else {

                            //prepare to display all previous guesses
                            char total_keyword [1000000];
                            memset(total_keyword, '\0', sizeof(total_keyword));
                            //copy first keyword
                            strncpy(total_keyword, guesses[current_player_num][0], strlen(guesses[current_player_num][0])+1);

                            //concat the rest of keywords
                            if(counter > 0){
                                int i = 1;
                                while(i <= counter){
                                    //separator between keywords
                                    strcat(total_keyword, ", ");
                                    //actual keyword
                                    strcat(total_keyword, guesses[current_player_num][i]);
                                    i++;
                                }
                            }

                            struct stat st;
                            stat("4_accepted.html", &st);
                            long added_length = strlen(total_keyword) + 7;

                            //get total size
                            long size = st.st_size + added_length;
                            n = sprintf(buff, HTTP_200_FORMAT, size);
                            
                            // send the header first
                            if (write(sockfd, buff, n) < 0){
                                perror("write");
                                return false;
                            }
                            
                            // read the content of the HTML file
                            int filefd = open("4_accepted.html", O_RDONLY);
                            n = read(filefd, buff, 2048);
                            if (n < 0) {
                                perror("read");
                                close(filefd);
                                return false;
                            }
                            close(filefd);

                            //find the split section to insert username
                            char* split = strstr(buff, "<form method=\"POST\">");
                            int split_length = strlen(split);
                            int buffer_length = strlen(buff);

                            //initialise response buffer to be 0
                            bzero(res_buff, 2049);

                            //copy html content until the split into the res buff
                            strncpy(res_buff, buff, buffer_length - split_length);

                            //add in the username into the res buff
                            strcat(res_buff, "<p>");
                            strcat(res_buff, total_keyword);
                            strcat(res_buff, "</p>");

                            //add in the other html after the split
                            strcat(res_buff, split);
                            if (write(sockfd, res_buff, size) < 0){
                                perror("write");
                                return false;
                            }
                        }
                    
                    //opponent keyword is at endgame or already started new game
                    } else if(state[opponent_cookie] == ENDGAME_ONE || state[opponent_cookie] == SECOND_ROUND){
                        if(!write_header_send_file("6_endgame.html", buff, HTTP_200_FORMAT, sockfd, n)){
                            return false;
                        }
                        state[cookie] = ENDGAME_ONE;
                    
                    //opponent not ready to start
                    } else if(state[opponent_cookie] < FIRST_ROUND){
                        if(!write_header_send_file("5_discarded.html", buff, HTTP_200_FORMAT, sockfd, n)){
                            return false;
                        }
                    }
                // no keyword in header implies the user quits
                } else {
                   if(!write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n)){
                        return false;
                    }
                    state[cookie] = GAMEOVER;
                    return false;
                }
            }

        // second round
        } else if (state[cookie] == SECOND_ROUND){
            // only post methods allowed in this page - either (quit or start with keyword)
            // check just for completeness
            if (method == POST){
                //logic if got keyword implies start, if not implies quit
                char* keyword = strstr(buff, "keyword=");
                if (keyword){
                    //implies both players are ready
                    if(state[opponent_cookie] == SECOND_ROUND){
                        keyword = strtok(keyword + 8, "&");

                        //copy keyword into guesses
                        int counter = next_guess_num(guesses, cookie);
                        strncpy(guesses[current_player_num][counter], keyword, strlen(keyword));
                        guesses[current_player_num][counter][strlen(keyword)+1] = '\0';

                        // check if user guessed the correct keyword of opponent
                        // if correct send endgame
                        if(validate_keyword(guesses[current_player_num][counter], guesses, opponent_player_num)){
                            if(!write_header_send_file("6_endgame.html", buff, HTTP_200_FORMAT, sockfd, n)){
                                return false;
                            }
                            state[cookie] = ENDGAME_TWO;

                        // keyword is an incorrect guess
                        } else {
                            char total_keyword [1000000];
                            memset(total_keyword, '\0', sizeof(total_keyword));

                            //copy first keyword
                            strncpy(total_keyword, guesses[current_player_num][0], strlen(guesses[current_player_num][0])+1);
                  
                            //concat the rest of the keywords
                            if(counter > 0){
                                int i = 1;
                                while(i <= counter){
                                    //separator between keywords
                                    strcat(total_keyword, ", ");
                                    //actual keywords
                                    strcat(total_keyword, guesses[current_player_num][i]);
                                    i++;
                                }
                            }

                            //get size of file
                            struct stat st;
                            stat("4b_accepted.html", &st);
                            long added_length = strlen(total_keyword) + 7;
                            long size = st.st_size + added_length;
                            n = sprintf(buff, HTTP_200_FORMAT, size);
                            
                            // send the header first
                            if (write(sockfd, buff, n) < 0){
                                perror("write");
                                return false;
                            }
                            
                            // read the content of the HTML file
                            int filefd = open("4b_accepted.html", O_RDONLY);
                            n = read(filefd, buff, 2048);
                            if (n < 0) {
                                perror("read");
                                close(filefd);
                                return false;
                            }
                            close(filefd);

                            //find the split section to insert username
                            char* split = strstr(buff, "<form method=\"POST\">");
                            int split_length = strlen(split);
                            int buffer_length = strlen(buff);

                            //initialise response buffer to be 0
                            bzero(res_buff, 2049);

                            //copy html content until the split into the res buff
                            strncpy(res_buff, buff, buffer_length - split_length);

                            //add in the username into the res buff
                            strcat(res_buff, "<p>");
                            strcat(res_buff, total_keyword);
                            strcat(res_buff, "</p>");

                            //add in the other html after the split
                            strcat(res_buff, split);
                            if (write(sockfd, res_buff, size) < 0){
                                perror("write");
                                return false;
                            }
                        }
                    
                    //opponent is at second endgame page already
                    } else if(state[opponent_cookie] == ENDGAME_TWO){
                        if(!write_header_send_file("6_endgame.html", buff, HTTP_200_FORMAT, sockfd, n)){
                            return false;
                        }
                        state[cookie] = ENDGAME_TWO;
                    
                    //opponent not ready to start
                    } else if(state[opponent_cookie] < SECOND_ROUND) {
                        if(!write_header_send_file("5b_discarded.html", buff, HTTP_200_FORMAT, sockfd, n)){
                            return false;
                        }
                    }
                // no keyword means the user quits
                } else {
                   if(!write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n)){
                        return false;
                    }
                    state[cookie] = GAMEOVER;
                    return false;
                }
            }
       
        // user are at endgame pages (either first round or second)
        } else if(state[cookie] == ENDGAME_ONE || state[cookie] == ENDGAME_TWO){
            //post for quit button
            if(method == POST){
                if(!write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n)){
                    return false;
                }
                state[cookie] = GAMEOVER;
                return false;
            
            //get for new game for second round (not handling third round)
            } else if (method == GET){
                //clear the keyword buffer for new game
                memset(guesses[current_player_num], '\0', sizeof(guesses[current_player_num]));
                if(!write_header_send_file("3b_first_turn.html", buff, HTTP_200_FORMAT, sockfd, n)){
                    return false;
                }
                state[cookie] = SECOND_ROUND;
            } 
        // undefined state return 404
        }else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0){
            perror("write");
            return false;
        }
    }
    return true;
}

/* get the cookie from a given header 
   if present return int from 0 to 9
   else return -1 
*/  
int get_cookie(char* buff){
    char* cookie_ptr;
    cookie_ptr = strstr(buff, "Cookie: ");
    int cookie = -1;
    if (cookie_ptr){
        cookie_ptr = cookie_ptr +8;

        //copy the next character as cookie ranges from 0 - 9 only
        if(isdigit(cookie_ptr[0])){
            //convert str 0-9 to int
            cookie = cookie_ptr[0] - '0';
        }
    }
    return cookie;
}

/* write the header and send the given file 
   return false if there's problem reading or writing file
*/
bool write_header_send_file(char* filename, char* buff, char const * format, int sockfd, int n){
    struct stat st;
    stat(filename, &st);
    n = sprintf(buff, format, st.st_size);
    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }
    // send the file
    int filefd = open(filename, O_RDONLY);
    do
    {
        n = sendfile(sockfd, filefd, NULL, 2048);
    }
    while (n > 0);
    if (n < 0)
    {
        perror("sendfile");
        close(filefd);
        return false;
    }
    close(filefd);
    return true;
}

/* get the next number of guesses */
int next_guess_num(char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD], int current_player_num){
    int counter = 0;
    while(counter < MAX_KEYWORD_NUM){
        if(strlen(guesses[current_player_num][counter]) == 0){
            break;
        }
        counter ++;
    }
    return counter;
}

/* get the next available player number
    either 0 or 1 since two players per game */
int next_player_num(int current_player_cookies[]){
    int counter = 0;
    while(counter < NUM_PLAYER){
        if(current_player_cookies[counter] == -1){
            return counter;
        }
        counter++;
    }
    return counter;
}

/* get the player number of the given cookie (either 0 or 1) */
int get_player_num(int cookie, int current_player_cookies[]){
    int player_num = 0;
    while(player_num < NUM_PLAYER){
        if(current_player_cookies[player_num] == cookie){
            break;
        }
        player_num++;
    }
    return player_num;
}

/* get opponent player number (either 0 or 1)*/
int get_opponent_cookie(int current_player_cookies[], int user_cookie){
    int counter = 0;
    while(counter < NUM_PLAYER){
        if(current_player_cookies[counter] != user_cookie){
            break;
        }
        counter++;
    }
    return current_player_cookies[counter];
}

/* clear all the guesses buffer and reinitialise player state to prepare for new game */
void reinitialise_player_state_and_guesses(int state[], int current_player_cookies[], 
    char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD]){
    if(state[current_player_cookies[0]] == GAMEOVER && state[current_player_cookies[1]] == GAMEOVER){
       for(int i = 0; i < NUM_PLAYER; i++){
            state[current_player_cookies[i]] = 0;
            memset(guesses[current_player_cookies[i]], '\0', sizeof(guesses[current_player_cookies[i]]));
            current_player_cookies[i] = -1;
       }
    }
}

/* store player in current_players_cookie in the game */
void register_player(int cookie, int current_player_cookies[]){
    if(current_player_cookies[0] != cookie && current_player_cookies[1] != cookie){
        int player_num = next_player_num(current_player_cookies);
        current_player_cookies[player_num] = cookie;
    }
}

/* clear buffer for guesses to prepare for new round */
void initialise_guesses(char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD]){
    for(int i = 0; i < NUM_PLAYER; i++){
        memset(guesses[i], '\0', sizeof(guesses[i]));
    }
}

/* return true if keyword made by user is in guesses of opponent,
   false otherwise
*/
bool validate_keyword(char keyword[], char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD], int opponent_player_num){
    int i = 0;
    while(i < MAX_KEYWORD_NUM){
        //no more keywords in the buffer can break from loop
        if(strlen(guesses[opponent_player_num][i]) == 0){
            break;
        }
        else if(strcmp(keyword, guesses[opponent_player_num][i]) == 0){
            return true;
        }
        i++;
    }
    return false;
}