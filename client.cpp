//header declarations
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

//definitions
#define PORT 2203

static void die(const char *msg){
    int err=errno;
    fprintf(stderr,"[%d]:%s\n",err,msg);
    abort();
}

int main(){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);//create a socket on client machine
    
    if(sockfd<0){
        die("socket() failed");
    }
    
    struct sockaddr_in addr={};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(PORT);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    socklen_t socklen=sizeof(addr);
    
    int conn_result=connect(sockfd,(struct sockaddr*)&addr,socklen);
    
    if(conn_result<0){
        die("connect() failed");
    }
    
    char write_buf[]="hello";
    int send_result=send(sockfd,write_buf,strlen(write_buf),0);
    if(send_result<0){
        die("send failed");
    }
    
    char read_buf[64]={};
    
    ssize_t recv_result = recv(sockfd,read_buf,sizeof(read_buf)-1,0);
    if(recv_result<0){
        die("send failed");
    }
    
    printf("server says %s\n",read_buf);
    
    return 0;
}