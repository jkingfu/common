#ifndef _CONFIGURATION_HPP_
#define _CONFIGURATION_HPP_
#include <string>
class configuration {

public:
    int port;
    std::string domain;
    std::string ip;
    int tcp_connect_timeout;
    int handler_thread_num;
    unsigned int thread_stack_size;
};

#endif
