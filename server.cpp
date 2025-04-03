//header declarations
#include <cstdint>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>

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

static int32_t read_full(int fd,char *buf,size_t n) { //read_full for more reliable read
    
    while(n>0){
        
        ssize_t rv=read(fd,buf,n);
        
        if(rv<=0){
            return -1; //error
        }
        
        assert(rv<=(ssize_t)n);
        n-=rv;
        buf+=rv;
    }
    return 0;
}

static int32_t write_all(int fd,const char *buf,size_t n) { //write_all for more reliable write  
    
    while(n>0){
        
        ssize_t rv=write(fd,buf,n);
        
        if(rv<=0){
            return -1; //error
        }
        
        assert(rv<=(ssize_t)n);
        n-=rv;
        buf+=rv;
    }
    return 0;
}

const size_t max_msg=4096; //maximum buffer size for payload

static int32_t one_request(int conn_sd){
   
    char rbuf[4+max_msg]={};
    errno=0;
    int32_t err=read_full(conn_sd,rbuf,4);
    if(err<0){
        msg(errno==0?"Connection closed by client":"read() error");//identifying error cases
        return -1;
    }
    
    uint32_t len=0;
    memcpy(&len,rbuf,4);
    if(len>max_msg){
        msg("too long");
        return -1;
    }
    
    err=read_full(conn_sd,rbuf+4,len);
    if(err<0){
        msg("read() error");
        return err;
    }
    
    fprintf(stderr,"client says: %.*s\n",len,rbuf+4);
    
    
    //reply to the request
    const char reply[]="world";
    char wbuf[4+sizeof(reply)]={};
    len=(uint32_t)strlen(reply);
    memcpy(wbuf,&len,4);
    memcpy(wbuf+4,reply,len);
    
    return write_all(conn_sd,wbuf,4+len);
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
        
        while(true){
            int32_t err=one_request(connection_sd);
            if(err<0){
                break;
            }
        }
        
        close(connection_sd);
    }
    
    return 0;
}