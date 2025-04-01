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

static void msg(const char *msg){
    fprintf(stderr,"%s\n",msg);
}

static void die(const char *msg){
    int err=errno;
    fprintf(stderr,"[%d]:%s\n",err,msg);
    abort();
}

static void do_something(int conn_sd){
    char read_buf[64]={};
    ssize_t recv_result=recv(conn_sd,read_buf,sizeof(read_buf)-1,0);
    if(recv_result<0){
        msg("recv() failed");
    }
    
    fprintf(stderr,"client says %s\n",read_buf);
    
    char write_buf[]="world";
    ssize_t send_result=send(conn_sd,write_buf,strlen(write_buf),0);
    if(send_result<0){
        msg("send() failed");
    }
}

int main() {

    int listening_sd=socket(AF_INET, SOCK_STREAM,0); //listening socket descriptor defined
    
    if(listening_sd<0){
        die("listening socket creation failed"); //error checking
    }
    
    int opt=1;
    setsockopt(listening_sd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));//set options
    
    struct sockaddr_in addr={};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(PORT);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    int bind_result = bind(listening_sd,(struct sockaddr*)&addr,sizeof(addr)); //bind
    
    if(bind_result < 0){
        die("bind failed");
    }
    
    int listen_value=listen(listening_sd,SOMAXCONN);
    
    if(listen_value<0)
    {
        die("listen failed");
    }
    
    while(true)
    {
        //accept
        struct sockaddr_in client_addr={};
        socklen_t addrlen=sizeof(client_addr);
        int connection_sd = accept(listening_sd, (struct sockaddr *)&client_addr, &addrlen);
        if(connection_sd<0){
            continue; //error, but continue accepting new connections
        }
        
        do_something(connection_sd);
        
        close(connection_sd);
    }
    
    return 0;
}