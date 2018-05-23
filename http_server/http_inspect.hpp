#ifndef _HTTP_INSPECT_HPP_
#define _HTTP_INSPECT_HPP_

#include <errno.h>
#include <unistd.h>
#include <task_base.hpp>
#include <pthread.h>

#include "http_acceptor.h"


class http_inspect : public task_base{
public:
    http_inspect(const http_acceptor* acceptor_):acceptor(acceptor_) {
      stop_task = 0;
    }
    ~http_inspect(){}
    int open(const configuration &config);
    virtual int stop();
    virtual int svc();
private:
    const http_acceptor* acceptor;
    int stop_task;

};

#endif
