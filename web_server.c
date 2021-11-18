#include<stdio.h>
#include<unistd.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<signal.h>
#include<string.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/stat.h>


void pkt_handler(int connfd);
void get_ftype(char *buf, char *ftype);
int get_body(char *buf);

#define SA struct sockaddr
#define BUF_SIZE 8192


int cp_listenfd;
int main(int argc, char **argv){
    int status;
    int listenfd, connfd;
    pid_t child_pid;
    socklen_t chilen;
    struct sockaddr_in cliaddr, servaddr;
    void sig_chld(int);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(12345);

    
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1} , sizeof(int)) < 0)
        perror("setsockopt");
    status = bind(listenfd, (SA *)&servaddr, sizeof(servaddr));
    if(status == -1){
        perror("Bind Error");
        exit(1);
    }
    status = listen(listenfd, 128);
    if(status == -1)printf("error:LISTEN\n");
    
    signal(SIGCHLD, sig_chld);
    
    cp_listenfd = listenfd;
    printf("\n-----Start listening-----\n");
    for(;;){
        chilen = sizeof(cliaddr);
        connfd = accept(listenfd, (SA *) &cliaddr, &chilen);
        if(connfd == -1)printf("error:ACCEPT\n");
        printf("Accept success!\n");
        if((child_pid = fork()) == 0){      //child process
            
            pkt_handler(connfd);
           
            return 0;
        }
        else close(connfd);
    }

}


// kill zombie child
void sig_chld(int sig_num){
    waitpid(-1, NULL, 0);
    return;
}


/*
GET & POST
*/
void pkt_handler(int connfd){
    char buf[BUF_SIZE+1];
    int size = 0, ret;
    char ftype[32];
    

    memset(buf, 0, BUF_SIZE+1); //init
    memset(ftype, 0, 32);
    ret = read(connfd, buf, BUF_SIZE);
    buf[ret] = 0;
    printf("buf = %s\n", buf);
 
    if(ret <= 0){
        printf("connet can't read\n");
        exit(1);
    }

    if(strncmp(buf, "GET", 3) == 0 || strncmp(buf, "get", 3) == 0){
        for(int i = 0; i < ret; i++){
            if(buf[i] == ' ' || buf[i] == '\r' || buf[i] == '\n')
                buf[i] = '\0';
        }
        int opfd;

        //get_ftype(&buf[5], ftype);
        char file[64] = {"."};
        if(buf[4] == '/' && buf[5] == '\0'){
            //strncpy(file, "index.html\0", 11);  
            opfd = open("./index.html", O_RDONLY);
            sprintf(buf,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
            write(connfd, buf, strlen(buf));
            
            int len = read(opfd, buf, BUF_SIZE);
            buf[len] = '\0';
            write(connfd, buf, len+1);
            close(opfd);
            return;
        }
        else{
            get_ftype(&buf[4], ftype);
            strcpy(&file[1], &buf[4]);
            sprintf(buf,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", ftype);
            write(connfd, buf, strlen(buf));
            printf("file = %s\n", file);
            
            opfd = open(file, O_RDONLY);
            if(opfd == -1){
                perror("open");
                printf("%s\n",file);
            }

            int ret;
            while((ret = read(opfd, buf, BUF_SIZE)) > 0){
                write(connfd, buf, ret);
            }

            close(opfd);
            return;
        }
        
        return;
    }
    else if(strncmp(buf, "POST", 4) == 0|| strncmp(buf, "post", 4) == 0){
        

        sprintf(buf,"HTTP/1.0 200 OK\r\n\r\n");
        write(connfd, buf, strlen(buf));
        char *front, *rear;
        // ret = read(connfd, buf, BUF_SIZE);
        // printf("buf = %d\n", ret);
        int opfd = open("upload.txt", O_CREAT|O_TRUNC|O_WRONLY, S_IRWXU|S_IRWXG|S_IRWXO);
        if(opfd == -1){
            perror("open");
        }
        while((ret = read(connfd, buf, BUF_SIZE-1)) > 0){
            int len = get_body(buf);
            write(opfd, buf, len);
            // printf("%s", buf);
        }
        close(opfd);
    }
    else{
        printf("Error type request\n");
        exit(1);
    }

}


/*
determine the type of file.
*/
void get_ftype(char *buf, char *ftype){

    char tmp[32];
    memset(tmp, 0, 32);

    for(int i = 0; i < 100; i++){
        if(buf[i] == '.'){
            strcpy(tmp, &buf[i+1]);
            break;
        }
    }
    if(strncmp(tmp, "gif", 3) == 0){
        strcpy(ftype, "image/gif\0");
    }
    else if(strncmp(tmp, "html", 4) == 0){
        strcpy(ftype, "text/html\0");
    }
    else if(strncmp(tmp, "jpeg", 4) == 0){
        strcpy(ftype, "image/jpeg\0");
    }
    else if(strncmp(tmp, "php", 3) == 0){
        strcpy(ftype, "php\0");
    }
    else{
        strcpy(ftype, "none\0");
    }
}

/*
get data from post_packet's body, which is ture file data.
*/
int get_body(char *buf){
    char tmp[BUF_SIZE+1];
    char *front = NULL, *rear = NULL;
    int num = 0, pos1, pos2;
    for(int i = 0; i < BUF_SIZE; i++){
        if(buf[i] == '\r'){
            num++;
            if(num == 4){
                pos1 = i+2;
                break;
            }
        }
    }
    for(int i = pos1+1; i < BUF_SIZE; i++){
        if(buf[i] == '\r'){
            pos2 = i-1;
            break;
        }
    }

    int len = pos2-pos1+1;
    strncpy(buf, &buf[pos1], len);
    buf[len] = '\0';
    return len;
}