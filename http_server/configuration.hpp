#ifndef _CONFIGURATION_HPP_
#define _CONFIGURATION_HPP_

class configuration {

public:
    int port;
    int max_conn;
    int handler_thread_num;
    unsigned int thread_stack_size;
    int epoll_cnt;
};

#endif
