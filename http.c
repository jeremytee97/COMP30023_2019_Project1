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

bool handle_http_request(int sockfd)
{
    printf("===sock fd %d", sockfd);
    // try to read the request
    char buff[2049];
    char res_buff[2049];
    int n = read(sockfd, buff, 2049);
    if (n <= 0)
    {
        if (n < 0)
            perror("read");
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
    // assume the only valid request URI is "/" but it can be modified to accept more files
    if (*curr == ' ')
        if (method == GET)
        {
            // get the size of the file
            struct stat st;
            stat("1_intro.html", &st);
            n = sprintf(buff, HTTP_200_FORMAT, st.st_size);
            // send the header first
            if (write(sockfd, buff, n) < 0)
            {
                perror("write");
                return false;
            }
            // send the file
            int filefd = open("1_intro.html", O_RDONLY);
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
        }
        else if (method == POST)
        {
            // locate the username and copy to another buffer using strncpy to ensure that 
            // it will not be overwritten.
            char * username = strstr(buff, "user=") + 5;
            int username_length = strlen(username);

            // user stores the username input
            char user[username_length+1];
            strncpy(user, username, username_length);
            user[username_length+1] = 0;
            
            // get the size of the file     
            struct stat st;
            stat("2_start.html", &st);
            
            n = sprintf(buff, HTTP_200_FORMAT, st.st_size);
            // send the header first
            if (write(sockfd, buff, n) < 0)
            {
                perror("write");
                return false;
            }
            
            // read the content of the HTML file
            int filefd = open("2_start.html", O_RDONLY);
            n = read(filefd, buff, 2048);
            if (n < 0)
            {
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
            strcat(res_buff, "<h3>");
            strcat(res_buff, user);
            strcat(res_buff, "</h3>");

            //add in the other html after the split
            strcat(res_buff, split);

            printf("%s", res_buff);
            if (write(sockfd, res_buff, st.st_size) < 0)
            {
                perror("write");
                return false;
            }
        }
        else {
            // never used, just for completeness
            fprintf(stderr, "no other methods supported");
        }       
    // send 404
    else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0)
    {
        perror("write");
        return false;
    }

    return true;
}

/* GET /?start=Start HTTP/1.1
Host: 172.26.37.123:7901
Connection: keep-alive
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/73.0.3683.86 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/