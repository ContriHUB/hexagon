//header declarations
#include <cerrno>
#include <cstddef>
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
#include <vector>
#include <sys/epoll.h>
#include <fcntl.h>

//definitions
#define PORT 2203

static void msg(const char *msg){
    fprintf(stderr,"%s\n",msg);
}

static void msg_errno(const char *msg){
    fprintf(stderr,"[errno:%d] %s\n",errno,msg);
}

static void die(const char *msg){
    int err=errno;
    fprintf(stderr,"[%d]: %s\n",err,msg);
    abort();
}

const size_t max_msg=32<<20;

struct Conn{
    int fd=-1;
    
    bool want_read=false;
    bool want_write=false;
    bool want_close=false;
    
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};

static void buf_consume(std::vector<uint8_t> &buf, size_t n){
    buf.erase(buf.begin(),buf.begin()+n);
}

static void buf_append(std::vector<uint8_t> &buf,const uint8_t * data,size_t len){
    buf.insert(buf.end(),data,data+len);
}

static void fd_set_nb(int fd){
    
    errno=0;
    int flags=fcntl(fd,F_GETFL,0);
    if(errno){
        die("fcntl() error");
    }
    
    flags |= O_NONBLOCK;
    errno=0;
    (void)fcntl(fd,F_SETFL,flags);
    if(errno){
        die("fcntl() error");
    }
}

static Conn* handle_accept(int fd){
    
    struct sockaddr_in client_addr={};
    socklen_t addrlen=sizeof(client_addr);
    int conn_fd=accept(fd,(struct sockaddr*)&client_addr,&addrlen);
    
    if(conn_fd<0){
        msg_errno("accept() error");
        return nullptr;
    }
    
    uint32_t ip=client_addr.sin_addr.s_addr;
    fprintf(stderr,"new incoming connection from %u.%u.%u.%u:%u",ip & 255,(ip>>8) & 255,(ip>>16) & 255,(ip>>24)&255,ntohs(client_addr.sin_port));
    
    fd_set_nb(conn_fd);
    Conn * conn=new Conn();
    conn->fd=conn_fd;
    conn->want_read=true;
    return conn;
}

static bool try_one_request(Conn *conn){
    if(conn->incoming.size()<4){
        return false;//want read
    }
    
    uint32_t len=0;
    memcpy(&len,conn->incoming.data(),4);
    if(len>max_msg){
        msg("too long");
        conn->want_close=true;
        return false;
    }
    
    if(4+len>conn->incoming.size()){
        return false;//want read
    }
    
    const uint8_t *request=&conn->incoming[4];
    //application logic
    printf("client says: len:%d data:%.*s\n",len,len<100?len:100,request);
    
    //echo response
    buf_append(conn->outgoing,(const uint8_t*)&len,4);
    buf_append(conn->outgoing,request,len);
    
    buf_consume(conn->incoming,(size_t)4+len);
    return true;
}

static void handle_write(Conn * conn){
    assert(conn->outgoing.size()>0);
    ssize_t rv=write(conn->fd,&conn->outgoing[0],conn->outgoing.size());
    if(rv<0 && errno==(EAGAIN|EWOULDBLOCK)){
        return;//Socket not ready
    }
    
    if(rv<0){
        msg_errno("write() error");
        conn->want_close=true;
        return;
    }
    
    buf_consume(conn->outgoing,(size_t)rv);
    
    if(conn->outgoing.size()==0){
        conn->want_read=true;
        conn->want_write=false;
    }//else want write
}

static void handle_read(Conn * conn){
    uint8_t buf[64*1024];
    ssize_t rv=read(conn->fd,buf,sizeof(buf));
    if(rv<0 && errno==(EAGAIN|EWOULDBLOCK)){
        return;//Socket not ready
    }
    
    if(rv<0){
        msg_errno("read() error");
        conn->want_close=true;
        return;
    }
    
    if(rv==0){
        if(!conn->incoming.size()){
            msg("client closed");
        } else{
            msg("unexpected EOF");
        }
        conn->want_close=true;
        return;
    }
    
    buf_append(conn->incoming,buf,(size_t)rv);
    
    while(try_one_request(conn)){}
    
    if(conn->outgoing.size()>0){
        conn->want_read=false;
        conn->want_write=true;
        return handle_write(conn);//Attempt to write to socket as usually the socket is ready to read and write both
    }// else want read
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
    
    fd_set_nb(listening_sd);
    
    int listen_value=listen(listening_sd,SOMAXCONN);
    
    if(listen_value<0)
    {
        die("listen failed");
    }
    
    std::vector<Conn*> fd2conn;
    std::vector<struct epoll_event> epoll_args; 
    
    while(true)
    {   
        epoll_args.clear();
        
        int epfd=epoll_create1(0);
        
        if(epfd<0){
            die("epoll()");
        }
        
        struct epoll_event listening_fd_ee;
        listening_fd_ee.events=EPOLLIN;
        listening_fd_ee.data.fd=listening_sd;
        
        epoll_ctl(epfd,EPOLL_CTL_ADD,listening_sd,&listening_fd_ee);
        
        int max_events=1;
        
        for(Conn* conn:fd2conn){
            if(!conn){
                continue;
            }
            
            struct epoll_event efd_event;
            efd_event.data.fd = conn->fd;
            efd_event.events=EPOLLERR;
            
            if(conn->want_write){
                efd_event.events|=EPOLLOUT;
            }
            
            if(conn->want_read){
                efd_event.events|=EPOLLIN;
            }
            
            max_events++;
            epoll_ctl(epfd,EPOLL_CTL_ADD,conn->fd,&efd_event);
        }
        
        epoll_args.resize(max_events);
        
        int val=epoll_wait(epfd,epoll_args.data(),max_events,-1);
        if(val==-1 && errno==EINTR){
            continue;
        }
        if(val<0){
            die("epoll()");
        }
        
        for(int i=0;i<val;i++){
            
            if(epoll_args[i].data.fd==listening_sd){
                if(epoll_args[i].events & EPOLLIN){
                    if(Conn *conn=handle_accept(listening_sd)){
                        if(fd2conn.size()<=(size_t)conn->fd){
                            fd2conn.resize(conn->fd+1);
                        }
                        assert(!fd2conn[conn->fd]);
                        fd2conn[conn->fd]=conn;
                    }
                }
                continue;
            }
            
            Conn* conn=(Conn *)fd2conn[epoll_args[i].data.fd];
            
            if(epoll_args[i].events & EPOLLIN){
                assert(conn->want_read);
                conn->want_read=true;
                handle_read(conn);
            }
            
            if(epoll_args[i].events & EPOLLOUT){
                assert(conn->want_write);
                conn->want_write=true;
                handle_write(conn);
            }
            
            if((epoll_args[i].events & EPOLLERR) || conn->want_close){
                (void)close(conn->fd);
                fd2conn[epoll_args[i].data.u32]=nullptr;
                delete(conn);
            }
        }
    }
    
    return 0;
}