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

static int32_t read_full(int fd,char *buf,size_t n) {
    
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

static int32_t write_all(int fd,const char *buf,size_t n) {
    
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

const size_t max_msg=4096;

static int32_t query(int conn_sd,const char* text){
    uint32_t len=(uint32_t)strlen(text);
    if(len>max_msg){
        return -1;
    }
    
    char wbuf[4+max_msg];
    memcpy(wbuf,&len,4);
    memcpy(wbuf+4,text,len);
    int32_t err=write_all(conn_sd,wbuf,4+len);
    if(err<0){
        return -1;
    }
    
    char rbuf[4+max_msg];
    errno=0;
    err=read_full(conn_sd, rbuf,4);
    if(err<0){
        msg(errno==0?"EOF":"read() failed");
        return err;
    }
    
    memcpy(&len,rbuf,4);
    if(len>max_msg){
        msg("too long");
        return -1;
    }
    
    err=read_full(conn_sd,rbuf+4,len);
    if(err<0)
    {
        msg("read() error");
        return -1;
    }
    
    printf("server says: %.*s\n",len,rbuf+4);
    return 0;
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
    
    int32_t err=query(sockfd,"hello1");
    if(err){
        goto L_DONE;
    }
    
    err=query(sockfd,"hello2");
    if(err){
        goto L_DONE;
    }
    
    err=query(sockfd,"hello3");
    if(err){
        goto L_DONE;
    }
    
L_DONE:
    close(sockfd);
    return 0;
}