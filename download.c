/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>

#include <string.h>

#define STANDARD_PORT 21

char* lastOcurrenceOf(char* code, char* response){
    char* lastOcurrence = NULL;
    char* currentOcurrence = response;
    while (1){
        currentOcurrence = strstr(currentOcurrence, code);
        if (currentOcurrence == NULL) break;
        lastOcurrence = currentOcurrence;
        currentOcurrence++;
    }
    return lastOcurrence;
}

int endWithNewLine(char* response){
    int length = strlen(response);
    if (length < 2) return 0;
    if (response[length - 2] == '\r' && response[length - 1] == '\n') return 1;
    return 0;
}

int isLastLine(char* response){
    char* lastOcurrence = lastOcurrenceOf("\r\n", response);
    lastOcurrence[0] = 0;
    char *lastOcurrenceOfLastOcurrence = lastOcurrenceOf("\r\n", response);
    lastOcurrence[0] = '\r';
    if (lastOcurrenceOfLastOcurrence == NULL) return response[3] == ' ';
    
    return lastOcurrenceOfLastOcurrence[5] == ' ';
}


int readResponse(int sockfd, char *response){
    char temp[1024];
    bzero(temp, 1024);
    char code[4];
    response[0] = 0;
    int bytes_read = read(sockfd, temp, 1024);
    if (bytes_read <= 0){
        printf("Error: Could not read response from server.\n");
        return -1;
    }
    strncpy(code, temp, 3);
    code[3] = 0;
    strcat(response, temp);
    while (1){
        if (endWithNewLine(response) && isLastLine(response)) break;
        bzero(temp, 1024);
        bytes_read = read(sockfd, temp, 1024);
        if (bytes_read <= 0){
            printf("Error: Could not read response from server.\n");
            return -1;
        }else if (bytes_read == 1024){
            printf("Error: Response too long.\n");
            return -1;
        }
        if (temp != NULL) strcat(response, temp);
    }
    return atoi(code);
}

// return -1 if error , 0 if success and 1 if no login (both 0 and 1 are success)
int parse(char* arg, char* user, char* password, char* host, char* path){
    // Check if arg starts with "ftp://"
    char tempchar = arg[6];
    arg[6] = 0;
    if (strcmp(arg, "ftp://") != 0){
        printf("Usage: download ftp://[<user>:<password>@]<host>/<url-path>");
        return -1;
    }
    arg[6] = tempchar;

    // check if arg contains login credentials
    int hasLogin = 0;
    int i = 6;
    while(1){
        char c = arg[i];
        i++;
        if (c == 0) break;
        if (c == '@'){
            hasLogin = 1;
            break;
        }
    }

    int currentIndex = 6;

    // extract login credentials
    if (hasLogin){
        // get user
        int index = 0;
        while(1){
            char temp = arg[currentIndex];
            currentIndex++;
            if (temp == 0) return -1;
            if (temp == ':'){
                user[index] = 0;
                break;
            }
            user[index] = temp;
            index++;
        }

        // get password
        int last_at_index = 0;
        index = currentIndex;
        while(1){
            char temp = arg[index];
            index++;
            if (temp == 0) break;
            if (temp == '@') last_at_index = index;
        }
        index = 0;
        while(1){
            char temp = arg[currentIndex];
            currentIndex++;
            if(currentIndex == last_at_index) break;
            password[index] = temp;
            index++;
        }
        password[index] = 0;

        // check if user and password are empty
        int length = strlen(user);
        if (length == 0){
            return -1;
        }
        length = strlen(password);
        if (length == 0){
            return -1;
        }
    }
    else{
        user[0] = 0;
        password[0] = 0;
    }

    // get host (everything between currentIndex and /)
    int index = 0;
    while(1){
        char temp = arg[currentIndex];
        currentIndex++;
        if (temp == 0) return -1;
        if (temp == '/') break;
        host[index] = temp;
        index++;
    }
    host[index] = 0;

    // get path (everything after /)(using strcpy)
    strcpy(path, arg + currentIndex);

    // check if host is empty
    int length = strlen(host);
    if (length == 0){
        return -1;
    }

    return hasLogin;
}

int openConnection(char* host, int port){
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(host);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}
          
int sendcredentials(int sockfd, char* user, char* password, char* path){
    char user_command[strlen(user) + 10];
    sprintf(user_command, "USER %s\r\n", user);
    printf("%s\n", user_command);
    write(sockfd, user_command, strlen(user_command));

    char response[1024*10];
    readResponse(sockfd, response);
    //check if response is good
    if (response[0] != '3'){
        printf("Unexpected response:\n %s", response);
        return -1;
    }
    printf("%s\n", response);

    char password_command[strlen(password) + 10];
    sprintf( password_command , "PASS %s\r\n", password);

    printf("%s\n", password_command);

    write(sockfd, password_command, strlen(password_command));

    readResponse(sockfd, response);
    //check if response is good
    if (response[0] != '2'){
        printf("Unexpected response:\n %s", response);
        return -1;
    }

    printf("%s\n", response);

    return 0;
}

int loginasanonymous(int sockfd){
    char user_command[] = "USER anonymous\r\n";
    printf("%s\n", user_command);
    write(sockfd, user_command, strlen(user_command));

    char response[1024];
    readResponse(sockfd, response);
    //check if response is good
    if (response[0] != '3'){
        printf("Unexpected response:\n %s", response);
        return -1;
    }

    printf("%s\n", response);

    char password_command[] = "PASS \r\n";
    printf("%s\n", password_command);
    write(sockfd, password_command, strlen(password_command));


    readResponse(sockfd, response);

    printf("%s\n", response);
    //check if response is good
    if (response[0] != '2'){
        printf("Unexpected response:\n %s", response);
        return -1;
    }
    return 0;
}

int parsePASV(char *response, char *ip, int *port){
    char *start = strchr(response, '(');
    char *end = strchr(response, ')');
    
    if (start != NULL && end != NULL) {
        // Increment start to skip the opening parenthesis '('
        start++;
        
        // Extracting the IP address and port numbers
        int h1, h2, h3, h4, p1, p2;
        sscanf(start, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
        
        // Format the IP address and port
        sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
        *port = (p1 << 8) + p2;
    }
    else {
        printf("Unexpected response:\n Invalid response to PASV command.\n");
        return -1;
    }
    return 0;
}



int main(int argc, char **argv) {

    if (argc > 2 || argc < 2){
        printf("Usage: download ftp://[<user>:<password>@]<host>/<url-path>\n");
        return -1;
    }
    
    int length = strlen(argv[1]);
    char user[length];
    char password[length];
    char hostname[length];
    char path[length];

    int has_login = 0;

    if ((has_login =parse(argv[1], user, password, hostname, path)) == -1){
        printf("Usage: download ftp://[<user>:<password>@]<host>/<url-path>");
        return -1;
    }

    // print extracted info
    if (has_login){
        printf("User: %s\n", user);
        printf("Password: %s\n", password);
    }
    printf("HostName: %s\n", hostname);
    printf("Path: %s\n", path);


    struct hostent *h;
    if ((h = gethostbyname(hostname)) == NULL){
        herror("gethostbyname()");
        exit(-1);
    }
    char host[1024];
    strcpy(host, inet_ntoa(*((struct in_addr *) h->h_addr)));

    printf("HostIPAddr:%s\n", host);

    printf("\nEstablishing connection to server...\n\n");

    // open connection
    int sockfd = openConnection(host, STANDARD_PORT);

    char response[10 * 1024];
    int response_code;

    if ((response_code = readResponse(sockfd, response)) == -1){
        return -1;
    }

    //check if response is good
    if (response_code != 220){
        printf("Unexpected response:\n %s", response);
        return -1;
    }

    printf("%s\n", response);

    //check if response is good
    if (response[0] != '2'){
        printf("Unexpected response:\n %s", response);
        return -1;
    }

    if (has_login){
        if (sendcredentials(sockfd, user, password, path) == -1){
            return -1;
        }
    }else{
        if (loginasanonymous(sockfd) == -1){
            return -1;
        }
    }

    write(sockfd, "PASV\r\n", 6);

    printf("PASV\n");

    if ((response_code = readResponse(sockfd, response)) == -1){
        return -1;
    }

    //check if response is good
    if (response[0] != '2' && response[0] != '1'){
        printf("Unexpected response:\n %s", response);
        return -1;
    }

    printf("%s\n", response);

    char ip[1024];
    int port;

    if (parsePASV(response, ip, &port) == -1){
        return -1;
    }

    int data_sockfd = openConnection(ip, port);

    char retr_command[length + 10];
    sprintf(retr_command, "RETR %s\r\n", path);
    printf("%s\n", retr_command);

    write(sockfd, retr_command, strlen(retr_command));

    if ((response_code = readResponse(sockfd, response)) == -1){
        return -1;
    }

    //check if response is good
    if ( response_code != 150 && response_code != 125){ // the second condition was missing for the second test
        printf("Unexpected response:\n %s", response);
        return -1;
    }

    printf("%s\n", response);    

    char filename[1024];
    char* lastbar = strrchr(path, '/');
    if (lastbar == NULL){
        strcpy(filename, path);
    }
    else strcpy(filename, lastbar + 1);

    FILE *file = fopen(filename, "w");

    char buffer[1024];
    int bytes_read;
    while ((bytes_read = read(data_sockfd, buffer, 1024)) > 0){
        fwrite(buffer, 1, bytes_read, file);
    }

    close(data_sockfd);
    // close the connection

    write(sockfd, "QUIT\r\n", 6);

    printf("QUIT\n");


    if ((response_code = readResponse(sockfd, response)) == -1){
        return -1;
    }

    printf("%s\n", response);

    printf("*** File downloaded successfully ***\n");

    fclose(file);

    close(sockfd);

    return 0;
}


