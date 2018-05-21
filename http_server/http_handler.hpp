#ifndef _HTTP_HANDLER_HPP
#define _HTTP_HANDLER_HPP

typedef struct {
    char* recv_buf;
    char* req_buf;
    char* decompress_buf;
} http_handler_args;
typedef struct {
    int fd;
    int local_epoll_idx;
} http_handle_args;
class http_handler { 
public:
    http_handler(const http_handler_args& handler_args_) : handler_args(handler_args_){}
    ~http_handler(){}
	
    int handle(const http_handle_args& handle_args_);
private:
    http_handler_args handler_args;
};

#endif
