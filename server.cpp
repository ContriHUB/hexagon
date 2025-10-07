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
#include <string>
#include <sys/epoll.h>
#include <fcntl.h>
#include <map>

//definitions
#define PORT 2203

static std::map<std::string,std::string> g_data; //placeholder, will be implemented

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
const size_t max_args=200*1000;

struct Conn{
    int fd=-1;
    
    bool want_read=false;
    bool want_write=false;
    bool want_close=false;
    
    // Efficient FIFO buffers for incoming/outgoing data
    // Use a sliding-head vector to allow O(1) amortized pop-front without memmove on every consume
    struct Buffer {
        std::vector<uint8_t> buf;
        size_t head = 0;

        size_t size() const {
            return buf.size() - head;
        }

        uint8_t* data() {
            return buf.data() + head;
        }

        const uint8_t* data() const {
            return buf.data() + head;
        }

        void append(const uint8_t* data_ptr, size_t len) {
            if (len == 0) return;
            buf.insert(buf.end(), data_ptr, data_ptr + len);
        }

        void consume(size_t n) {
            head += n;
            if (head > buf.size()) {
                head = buf.size();
            }
            // Compact when head grows large to reclaim space; amortized O(1)
            if (head >= 4096 && head * 2 >= buf.size()) {
                buf.erase(buf.begin(), buf.begin() + head);
                head = 0;
            }
        }
    };

    Buffer incoming;
    Buffer outgoing;
};

struct Response{
    uint32_t status=0;
    uint32_t len=0;
    uint8_t *data=nullptr;
};

// Removed vector-based FIFO helpers; replaced by Conn::Buffer methods

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

enum {
    RES_OK=0, //ok response
    RES_ERR=1, // error
    RES_NX=2 //not found
};

static bool read_u32(const uint8_t * &curr, const uint8_t *end,uint32_t &out){
    if(curr+4>end){
        return false;
    }
    
    memcpy(&out,curr,4);
    curr+=4;
    return true;
}

static bool read_str(const uint8_t* &curr,const uint8_t *end,size_t n,std::string &out){
    if(curr+n>end){
        return false;
    }
    
    out.assign(curr,curr+n);
    curr+=n;
    return true;
}

static int32_t parse_req(const uint8_t* data,size_t size,std::vector<std::string> &out){
    const uint8_t *end=data+size;
    uint32_t nstr=0;
    if(!read_u32(data,end,nstr)){
        return -1;
    }
    
    if(nstr>max_args){
        return -1;
    }
    
    while(out.size()<nstr){
        uint32_t len=0;
        if(!read_u32(data,end,len)){
            return -1;
        }
        out.push_back(std::string());
        if(!read_str(data,end,len,out.back())){
            return -1;
        }
    }
    
    if(data!=end){
        return -1;
    }
    
    return 0;
}

static void make_response(Response &resp,std::vector<uint8_t> &out){
    uint32_t resp_len=4+resp.len;
    // Adapt to Conn::Buffer when used
    // This overload remains for compatibility when called with a raw vector, but in this code
    // path we will pass Conn::Buffer, so an overload below handles it.
    out.insert(out.end(), (const uint8_t*)&resp_len, (const uint8_t*)&resp_len + 4);
    out.insert(out.end(), (const uint8_t*)&resp.status, (const uint8_t*)&resp.status + 4);
    if(resp.len>0) out.insert(out.end(), (const uint8_t*)resp.data, (const uint8_t*)resp.data + resp.len);
}

// Overload for Conn::Buffer
static void make_response(Response &resp, Conn::Buffer &out){
    uint32_t resp_len=4+resp.len;
    out.append((const uint8_t*)&resp_len,4);
    out.append((const uint8_t*)&resp.status,4);
    if(resp.len>0) out.append((const uint8_t*)resp.data,resp.len);
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
    fprintf(stderr,"new incoming connection from %u.%u.%u.%u:%u\n",ip & 255,(ip>>8) & 255,(ip>>16) & 255,(ip>>24)&255,ntohs(client_addr.sin_port));
    
    fd_set_nb(conn_fd);
    Conn * conn=new Conn();
    conn->fd=conn_fd;
    conn->want_read=true;
    return conn;
}

static void do_request(Response &resp, std::vector<std::string> &cmd){
    
    resp.status=0;
    
    if(cmd.size()==2 && cmd[0]=="get"){
        auto it=g_data.find(cmd[1]);
        if(it==g_data.end()){
            resp.status=RES_NX;
            return;
        }
        resp.len=it->second.size();
        resp.data=(uint8_t*)it->second.data();
    }
    else if(cmd.size()==3 && cmd[0]=="set"){
        g_data[cmd[1]].swap(cmd[2]);
    }
    else if(cmd.size()==2 && cmd[0]=="del"){
        g_data.erase(cmd[1]);
    }
    else{
        resp.status=RES_ERR;
    }
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
    
    const uint8_t *request=conn->incoming.data()+4;
    //application logic
    std::vector<std::string> cmd;
    if(parse_req(request,len,cmd)<0){
        conn->want_close=true;
        return false;
    }
    Response resp;
    do_request(resp,cmd);
    make_response(resp,conn->outgoing);
    
    conn->incoming.consume((size_t)4+len);
    return true;
}

static void handle_write(Conn * conn){
    assert(conn->outgoing.size()>0);
    ssize_t rv=write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if(rv<0 && errno==(EAGAIN|EWOULDBLOCK)){
        return;//Socket not ready
    }
    
    if(rv<0){
        msg_errno("write() error");
        conn->want_close=true;
        return;
    }
    
    conn->outgoing.consume((size_t)rv);
    
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
    
    conn->incoming.append(buf,(size_t)rv);
    
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