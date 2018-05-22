#ifndef _CONFIGURATION_HPP_
#define _CONFIGURATION_HPP_
#include <string>
class configuration {

public:
    int port;
    std::string ip;
    int handler_thread_num;
    unsigned int thread_stack_size;
};

#endif
