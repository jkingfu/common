#ifndef _HTTP_CLIENT_HPP_
#define _HTTP_CLIENT_HPP_

#include <errno.h>
#include <unistd.h>
#include <task_base.hpp>
#include <pthread.h>
#include "configuration.hpp"


class http_client : public task_base{
public:
    http_client(){stop_task=0;}
    ~http_client();
    
    int open(const configuration &config);
    virtual int stop();
    virtual int svc();
private:
    const configuration* config;
    int stop_task;
};

#endif
