#ifndef _HTTP_ACCEPTOR_HPP_
#define _HTTP_ACCEPTOR_HPP_

#include <errno.h>
#include <unistd.h>
#include <task_base.hpp>
#include <pthread.h>
#include <sys/epoll.h>
#include "configuration.hpp"


class http_acceptor : public task_base{
public:
    http_acceptor(int epoll_cnt, int max_conn);
    ~http_acceptor();
    
    int open(const configuration &config);
    virtual int stop();
    virtual int svc();
    
    int recycle_fd(int fd, int epoll_idx);
    int close_fd(int fd);

protected:
    int create_listen(int &socket_fd, unsigned short port);
    int set_socket(int fd, int flag);
    int add_input_fd(int fd, int efd);
    int del_input_fd(int fd, int efd);
    int create_pipe(int idx);
    uint32_t genWid();

protected:
    int max_conn;
    
    int listen_port;
    int listen_fd;
    int* epoll_fds;
    int stop_task;
    int conn_num;
    uint32_t wid;
    pthread_mutex_t* epoll_mutexes;
    pthread_mutex_t conn_mutex;
    pthread_mutex_t epoll_idx_mutex;
    epoll_event** epoll_ready_events;    
    int* pipe_read_fds;
    int* pipe_write_fds;
    int epoll_thread_cnt;
    
    uint32_t add_sum;
public:
    int epoll_cnt;
    int* epoll_ready_event_num;
    int  incoming_fd_cnt;
};

#endif
