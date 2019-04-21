#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool handle_http_request(int sockfd, int state[], char guesses[][20][101])
{
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
            char* cookie;
            cookie = get_cookie(curr);
            long size;

            //check whether it has cookie in the header
            if(cookie){
                // get the size of the file
                stat("2_start.html", &st);
                filefd = open("2_start.html", O_RDONLY);
                size = st.st_size + strlen(cookie) + 9;
                printf("strlen(cookie) %ld", strlen(cookie));
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

            bzero(res_buff, 2048);

            int buffer_length = strlen(buff);

            if (cookie){
                //find the split section to insert username
                char* split = strstr(buff, "<form method=\"GET\">");
                int split_length = strlen(split);
                strncpy(res_buff, buff, buffer_length - split_length);
                strcat(res_buff, "<p>");
                strcat(res_buff, cookie);
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

            // user stores the username input
            char user[username_length+1];
            strncpy(user, username, username_length);
            user[username_length+1] = 0;
            long added_length = username_length +7;
            
            // get the size of the file     
            struct stat st;
            stat("2_start.html", &st);
            long size = st.st_size + added_length;
            n = sprintf(buff, HTTP_200_FORMAT_COOKIE, size, user);
            
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
            strcat(res_buff, user);
            strcat(res_buff, "</p>");

            //add in the other html after the split
            strcat(res_buff, split);
            if (write(sockfd, res_buff, size) < 0){
                perror("write");
                return false;
            }

            state[sockfd%2] = 1;
        }
    } else if (state[sockfd%2] == 2){
        // only post methods allowed in this page - either (quit or start with keyword)
        // check just for completeness
        if (method == POST){
            //logic if got keyword implies start, if not implies quit
            char* keyword = strstr(buff, "keyword=");
            if (keyword){
                keyword = strtok(keyword + 8, "&");
                printf("Keyword is %s %ld", keyword, strlen(keyword));
                int counter = next_guess_num(guesses, sockfd);
                printf("\nCounter %d\n", counter);
                strncpy(guesses[sockfd%2][counter], keyword, strlen(keyword));
                guesses[sockfd%2][counter][strlen(keyword)+1] = '0';
                printf("\nGuesses %s\n", guesses[sockfd%2][counter]);

                if(state[(sockfd%2)+1] == 2 || state[(sockfd%2)+1] == 3){
                    write_header_send_file("4_accepted.html", buff, HTTP_200_FORMAT, sockfd, n);
                } else {
                    write_header_send_file("5_discarded.html", buff, HTTP_200_FORMAT, sockfd, n);
                }
            } else {

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


char* get_cookie(char* buff){
    char* cookie_ptr;
    cookie_ptr = strstr(buff, "Cookie: ");
    char* cookie = NULL;
    if (cookie_ptr){
        cookie_ptr = cookie_ptr +8;
        int counter = 0;

        //find the end of the cookie value
        while(*cookie_ptr != '\r'){
            cookie_ptr++;
            counter +=1;
        }

        //copy the cookie value for safe keeping
        cookie_ptr = strstr(buff, "Cookie: ") +8;
        printf("Counter %d", counter);
        cookie = (char*)malloc(sizeof(char)*(counter +1));
        strncpy(cookie, cookie_ptr, counter);
        cookie[counter + 1] = 0;
        printf("%s", cookie);
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


int next_guess_num(char guesses[][20][101], int sockfd){
    int counter = 0;
    while(counter < 20){
        if(guesses[sockfd%2][counter][0] == '\0'){
            break;
        }
        counter ++;
    }
    return counter;
}
/*quit=Quit
else {
            // never used, just for completeness
            fprintf(stderr, "no other methods supported");
        }  
keyword=asda&guess=Guess */