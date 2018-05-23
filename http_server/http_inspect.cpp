#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <service_log.hpp>
#include "http_inspect.hpp"

int http_inspect::stop() {
    stop_task = 1;
    join();
    _INFO("http_inspect stops");
    return 0;
}

int http_inspect::open(const configuration &config) {
    return task_base::open(1, config.thread_stack_size);
}

int http_inspect::svc() {
    struct timeval t_start, t_end;
    long timeuse_s;
    long timeuse_us;
    int sleep_us= 60*1000*1000;
    int last_incoming_fd_cnt = 0;
    while (!stop_task) {
        gettimeofday( &t_start, NULL );
        usleep(sleep_us);      
        
        int epoll_cnt = acceptor->epoll_cnt;
        int* epoll_ready_event_num = acceptor->epoll_ready_event_num;
        int  incoming_fd_cnt = acceptor->incoming_fd_cnt;
        
        for (int i = 0; i < epoll_cnt; ++i) {
            _INFO("[http_inspect] %d=>%d", i, epoll_ready_event_num[i]);
        }
        
        gettimeofday( &t_end, NULL );
        timeuse_s = t_end.tv_sec - t_start.tv_sec;
        timeuse_us = 1000000 * (t_end.tv_sec - t_start.tv_sec ) + t_end.tv_usec - t_start.tv_usec;
        int incre = incoming_fd_cnt-last_incoming_fd_cnt;
        float qps = (float)incre/timeuse_s;
        _INFO("[http_inspect] qps %d/%lld=%f", incre, timeuse_s, qps);
        last_incoming_fd_cnt = incoming_fd_cnt;
    }
    return 0;
}
