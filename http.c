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
        else
            printf("socket %d close the connection\n", sockfd);
        return false;
    }

    // terminate the string
    buff[n] = 0;

    char * curr = buff;
    printf("%s", buff);
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

    
    if(*curr == 'f'){
       if(write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0){
            perror("write");
            return false;
       }
    }
    int cookie = get_cookie(curr);
    printf("Current cookie %d\n", cookie);
    // new player as no cookie was detected
    if(cookie < 0){
        if (method == GET){
             write_header_send_file("1_intro.html", buff, HTTP_200_FORMAT, sockfd, n);   

        } else if(method == POST){
            // locate the username 
            char * username = strstr(buff, "user=") + 5;
            int username_length = strlen(username);
            
            //get a unique cookie number for the user
            int cookieCounter = 0;
            while(cookieCounter < 20){
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
            state[cookieCounter] = 1;

            //assign player 0 or player 1
            int player_num = next_player_num(current_player_cookies);
            current_player_cookies[player_num] = cookieCounter;
        }
    // cookie is present 
    } else {
        //check if user has added itself as a current player and assign them as either player 0/1
        register_player(cookie, current_player_cookies);

        int opponent_cookie = get_opponent_cookie(current_player_cookies, cookie);
        //check if opponent has quit, if yes then gameover
        printf("\nOPPONENT COOKIE %d\n", opponent_cookie);
        if(opponent_cookie >= 0 && state[opponent_cookie] == 5){
            state[cookie] = 5;
            write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n);
            return false;
        }
        else if(state[cookie] == 0){
            state[cookie] = 1;
            int filefd;
            struct stat st;
            long size;

            // get the size of the file
            stat("2_start.html", &st);
            filefd = open("2_start.html", O_RDONLY);
            size = st.st_size + strlen(user_cookie_mapping[cookie]) + 9;
            printf("Current size is %ld\n",size);

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
        } else if (state[cookie] == 1){
            if (method == GET){
                // get the size of the file
                if(!write_header_send_file("3_first_turn.html", buff, HTTP_200_FORMAT, sockfd, n)){
                    return false;
                }
                state[cookie] = 2;

            } else if (method == POST){
                if(!write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n)){
                    return false;
                }
                state[cookie] = 5;
            }

        } else if (state[cookie] == 2){
            // only post methods allowed in this page - either (quit or start with keyword)
            // check just for completeness
            if (method == POST){
                //logic if got keyword implies start, if not implies quit
                char* keyword = strstr(buff, "keyword=");
                if (keyword){

                    //implies both players are ready
                    if(state[opponent_cookie] == 2){
                        keyword = strtok(keyword + 8, "&");
                        int counter = next_guess_num(guesses, cookie);
                        printf("\nCounter %d and strlen is %ld\n", counter, strlen(keyword));
                        strncpy(guesses[cookie][counter], keyword, strlen(keyword));
                        guesses[cookie][counter][strlen(keyword)+1] = '\0';
                        printf("\nCurrent guess %s\n", guesses[cookie][counter]);

                        char total_keyword [1000000];
                        memset(total_keyword, '\0', sizeof(total_keyword));
                        strncpy(total_keyword, guesses[cookie][0], strlen(guesses[cookie][0])+1);
                        struct stat st;
                        stat("4_accepted.html", &st);

                        if(counter > 0){
                            int i = 1;
                            printf("Current counter %d\n", counter);
                            while(i <= counter){
                                printf("For i = %d, the key is %s \n", i, guesses[cookie][i]);
                                strcat(total_keyword, ", ");
                                strcat(total_keyword, guesses[cookie][i]);
                                i++;
                            }
                        }

                        printf("ALL KEYWORD : %s \n", total_keyword);
                        long added_length = strlen(total_keyword) + 7;
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
                    } else {
                        write_header_send_file("5_discarded.html", buff, HTTP_200_FORMAT, sockfd, n);
                    }
                // no keyword means the user quits
                } else {
                    write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n);
                    state[cookie] = 5;
                    return false;
                }
            }
        // undefined state return 404
        } else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0){
            perror("write");
            return false;
        }
    }
    return true;
}
















/* 
    // Use the states to check which page the user are
    if (state[sockfd%2] == 1){
        if (method == GET){
             // get the size of the file
            if(!write_header_send_file("3_first_turn.html", buff, HTTP_200_FORMAT, sockfd, n)){
                return false;
            }
            state[sockfd%2] = 2;
        }

        else if (method == POST){
            if(!write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n)){
                return false;
            }
            state[sockfd%2] = 5;
        }
    } else if (state[sockfd%2] == 0){
        if (method == GET){
            int filefd;
            struct stat st;
            int cookie = get_cookie(curr);
            long size;

            //check whether it has cookie in the header
            if(cookie >= 0){
                // get the size of the file
                stat("2_start.html", &st);
                filefd = open("2_start.html", O_RDONLY);
                size = st.st_size + strlen(user_cookie_mapping[cookie]) + 9;
                state[sockfd%2] = 1;
            } else{
                // get the size of the file
                stat("1_intro.html", &st);
                filefd = open("1_intro.html", O_RDONLY);
                size = st.st_size;
                state[sockfd%2] = 0;
            }

            n = sprintf(buff, HTTP_200_FORMAT, size);

            // send the header first
            if (write(sockfd, buff, n) < 0)
            {
                perror("write");
                return false;
            }

            // send the file
            bzero(buff, 2049);
            n = read(filefd, buff, 2048);

            if (n < 0)
            {
                perror("read");
                close(filefd);
                return false;
            }
            close(filefd);

            //clear response buffer
            bzero(res_buff, 2048);
            int buffer_length = strlen(buff);

            if (cookie >= 0){
                //find the split section to insert username
                char* split = strstr(buff, "<form method=\"GET\">");
                int split_length = strlen(split);
                strncpy(res_buff, buff, buffer_length - split_length);
                strcat(res_buff, "<p>");
                strcat(res_buff, user_cookie_mapping[cookie]);
                strcat(res_buff, "</p>");

                //add in the other html after the split
                strcat(res_buff, split);
            } else{
                strncpy(res_buff, buff, buffer_length);
            }

            if (write(sockfd, res_buff, size) < 0){
                perror("write");
                return false;
            }
        } else if (method == POST) {
            // locate the username and copy to another buffer using strncpy to ensure that 
            // it will not be overwritten (Special case as the cookie is posted)
            char * username = strstr(buff, "user=") + 5;
            int username_length = strlen(username);
            
            //store cookie in user_cookie_mapping
            int cookieCounter = 0;
            while(cookieCounter < 20){
                if(strlen(user_cookie_mapping[cookieCounter]) == 0){
                    break;
                }
                cookieCounter++;
            }

            // user stores the username input
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

            state[sockfd%2] = 1;
        }
    //confused logic right here fix tmr
    } else if (state[sockfd%2] == 2){
        // only post methods allowed in this page - either (quit or start with keyword)
        // check just for completeness
        if (method == POST){
            //logic if got keyword implies start, if not implies quit
            char* keyword = strstr(buff, "keyword=");
            if (keyword){

                //implies both players are ready
                if(state[(sockfd+1)%2] == 2){
                    
                    keyword = strtok(keyword + 8, "&");
                    int counter = next_guess_num(guesses, sockfd);
                    printf("\nCounter %d and strlen is %ld\n", counter, strlen(keyword));
                    strncpy(guesses[sockfd%2][counter], keyword, strlen(keyword));
                    guesses[sockfd%2][counter][strlen(keyword)+1] = '\0';
                    printf("\nCurrent guess %s\n", guesses[sockfd%2][counter]);

                    char total_keyword [1000000];
                    memset(total_keyword, '\0', sizeof(total_keyword));
                    strncpy(total_keyword, guesses[sockfd%2][0], strlen(guesses[sockfd%2][0])+1);
                    struct stat st;
                    stat("4_accepted.html", &st);

                    if(counter > 0){
                        int i = 1;
                        printf("Current counter %d\n", counter);
                        while(i <= counter){
                            printf("For i = %d, the key is %s \n", i, guesses[sockfd%2][i]);
                            strcat(total_keyword, ", ");
                            strcat(total_keyword, guesses[sockfd%2][i]);
                            i++;
                        }
                    }

                    printf("ALL KEYWORD : %s \n", total_keyword);
                    long added_length = strlen(total_keyword) + 7;
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
                } else {
                    write_header_send_file("5_discarded.html", buff, HTTP_200_FORMAT, sockfd, n);
                }
            // a player quits so move to game over
            } else {
                write_header_send_file("7_gameover.html", buff, HTTP_200_FORMAT, sockfd, n);
                state[sockfd%2] = 5;
            }
        }
    } 
    // send 404
    else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0){
        perror("write");
        return false;
    }
    return true;
}
 */

int get_cookie(char* buff){
    char* cookie_ptr;
    cookie_ptr = strstr(buff, "Cookie: ");
    int cookie = -1;
    if (cookie_ptr){
        cookie_ptr = cookie_ptr +8;

        //copy the next character as cookie ranges from 0 - 9 only
        if(isdigit(cookie_ptr[0])){
            cookie = cookie_ptr[0] - '0';
        }
    }
    return cookie;
}


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


int next_guess_num(char guesses[][20][101], int cookie){
    int counter = 0;
    while(counter < 20){
        if(guesses[cookie][counter][0] == '\0'){
            break;
        }
        printf("first char is %c\n",guesses[cookie][counter][0]);
        counter ++;
    }
    return counter;
}

int next_player_num(int current_player_cookies[]){
    int counter = 0;
    while(counter < NUM_USER){
        if(current_player_cookies[counter] == -1){
            return counter;
        }
        counter++;
    }
    return counter;
}

int get_opponent_cookie(int current_player_cookies[], int user_cookie){
    int counter = 0;
    while(counter < NUM_USER){
        if(current_player_cookies[counter] != user_cookie){
            break;
        }
        counter++;
    }
    return current_player_cookies[counter];
}

void reinitialise_player_state_and_guesses(int state[], int current_player_cookies[], 
    char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD]){
    if(state[current_player_cookies[0]] == 5 && state[current_player_cookies[1]] == 5){
       for(int i = 0; i < NUM_USER; i++){
            state[current_player_cookies[i]] = 0;
            memset(guesses[current_player_cookies[i]], '\0', sizeof(guesses[current_player_cookies[i]]));
            current_player_cookies[i] = -1;
       }
    }
}

void register_player(int cookie, int current_player_cookies[]){
    if(current_player_cookies[0] != cookie && current_player_cookies[1] != cookie){
        int player_num = next_player_num(current_player_cookies);
        current_player_cookies[player_num] = cookie;
    }
}

void initialise_guesses(char guesses[][MAX_KEYWORD_NUM][MAX_SIZE_OF_KEYWORD]){
    for(int i = 0; i < MAX_COOKIE; i++){
        memset(guesses[i], '\0', sizeof(guesses[i]));
    }
}

/*quit=Quit
else {
            // never used, just for completeness
            fprintf(stderr, "no other methods supported");
        }  
keyword=asda&guess=Guess */