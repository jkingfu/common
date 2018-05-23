#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <service_log.hpp>
#include "http_acceptor.hpp"
#include "http_handler.hpp"

#define SOCKET_SND_BUF_SIZE (1024*1024)
#define SOCKET_RCV_BUF_SIZE (1024*1024)
#define max_recv_buf_len (1024 * 1024)
#define decompress_buf_len (10 * 1024 * 1024)
#define http_header_max_length 1024

http_acceptor::http_acceptor(int _epoll_cnt, int _max_conn) {
    epoll_cnt = _epoll_cnt;
    max_conn = _max_conn;
    _INFO("epoll_cnt=%d,max_conn=%d", epoll_cnt, max_conn);
    listen_port = -1;
    stop_task = 0;
    conn_num = 0;
    wid = 0;
    epoll_fds = new int[epoll_cnt]; // epoll fd
    epoll_ready_event_num = new int[epoll_cnt];  // ready (need process ) fd in cur epoll fd 
    pipe_read_fds = new int[epoll_cnt];  // catch signal
    pipe_write_fds = new int[epoll_cnt]; // catch signal
    add_sum = 2;
    for (int i = 0; i < epoll_cnt; i++) {
        epoll_ready_event_num[i] = 0;
    }
    epoll_mutexes = new pthread_mutex_t[epoll_cnt];
    for (int i = 0; i < epoll_cnt; i++) {
        pthread_mutex_init(&epoll_mutexes[i], NULL);   // lock thread in same epoll_cnt;use epoll_ready_event_num operate and epoll_ready_events operate
    }
    epoll_ready_events = new epoll_event*[epoll_cnt];  // and epoll_ready_event_num ,implement epoll event (ready)
    for (int i = 0; i < epoll_cnt; i++) {
        epoll_ready_events[i] = new epoll_event[max_conn];
    }
    pthread_mutex_init(&epoll_idx_mutex, NULL); // lock all handler thread when thread select epoll id, after not need this lock, use epoll_thread_cnt operate
    incoming_fd_cnt = 0; // no lock improve efficient
    epoll_thread_cnt = 0; // increment for select a epoll id every thread
    pthread_mutex_init(&conn_mutex, NULL); // lock listen thread, calc conn count
}

http_acceptor::~http_acceptor() {
    delete[] epoll_fds;
    delete[] epoll_ready_event_num;
    delete[] pipe_read_fds;
    delete[] pipe_write_fds;
    for (int i = 0; i < epoll_cnt; i++) {
    pthread_mutex_destroy(&epoll_mutexes[i]);
    }
    delete[] epoll_mutexes;
    for (int i = 0; i < epoll_cnt; i++) {
        delete[] epoll_ready_events[i];
    }
    delete[] epoll_ready_events;
    pthread_mutex_destroy(&epoll_idx_mutex);
    pthread_mutex_destroy(&conn_mutex);
}


int http_acceptor::recycle_fd(int fd, int epoll_idx) {
    if (epoll_idx < 0 || epoll_idx >= epoll_cnt) {
        _ERROR("err epollidx=%d", epoll_idx);
        return -1;
    }
    return add_input_fd(fd, epoll_fds[epoll_idx]);
}

int http_acceptor::close_fd(int fd) {
    int tmp;
    pthread_mutex_lock(&conn_mutex);
    conn_num--;
    tmp = conn_num;
    pthread_mutex_unlock(&conn_mutex);
    _INFO("close fd=%d, conn_num=%d", fd, tmp);
    return ::close(fd);
}

int http_acceptor::open(const configuration& config) {

    listen_port = config.port;

    for (int i = 0; i < epoll_cnt; i++) {
        epoll_fds[i] = epoll_create(max_conn);
        if (epoll_fds[i] == -1) {
            _ERROR("epoll create err on idx=%d", i);
            return -1;
        }
    }
    for (int i = 0; i < epoll_cnt; i++) {
        if (create_pipe(i) < 0) {
            _ERROR("create pipe err on idx=%d", i);
            return -1;
        }
        add_input_fd(pipe_read_fds[i], epoll_fds[i]);
    }

    if (create_listen(listen_fd, listen_port)) {
        _ERROR("create listen fail on port=%d", listen_port);
        return -1;
    }
    
    add_input_fd(listen_fd, epoll_fds[0]);

    _INFO("start thread num=%d", config.handler_thread_num * epoll_cnt);
    return task_base::open(config.handler_thread_num * epoll_cnt, config.thread_stack_size);
}

int http_acceptor::stop() {
    for (int i = 0; i < epoll_cnt; i++) {
        int stop = 1;
        write(pipe_write_fds[i], &stop, 1);
    }
    stop_task = 1;
    join();
    _INFO("http_handler stops");
    return 0;
}


int http_acceptor::svc() {
    int local_thd_idx = 0;
    int local_epoll_idx = 0;
    
    /**
    *Assign each thread an epoll fd, id is  local_epoll_idx
    */
    pthread_mutex_lock(&epoll_idx_mutex);
    local_epoll_idx = epoll_thread_cnt;
    local_thd_idx = epoll_thread_cnt;
    epoll_thread_cnt++;
    pthread_mutex_unlock(&epoll_idx_mutex);
    local_epoll_idx = local_epoll_idx % epoll_cnt;

    /**
    *  get epoll_fd epoll_events epoll_event_num epoll_mutex epoll by local_epoll_idx
    */
    int epoll_fd = epoll_fds[local_epoll_idx];
    epoll_event* es = epoll_ready_events[local_epoll_idx];
    pthread_mutex_t* epoll_mutex = &epoll_mutexes[local_epoll_idx];
    int* event_num = &epoll_ready_event_num[local_epoll_idx];

    char* recv_buf = (char*)malloc(max_recv_buf_len);
    char* req_buf = (char*)malloc(max_recv_buf_len);
    char* decompress_buf = (char*)malloc(decompress_buf_len);
    http_handler_args handler_args;
    handler_args.recv_buf=recv_buf;
    handler_args.req_buf=req_buf;
    handler_args.decompress_buf=decompress_buf;
    http_handler* handler= new http_handler(handler_args);
    int ret =-1;
    int fd, new_fd;
    uint32_t event;
    int tmp_conn;
    sockaddr_in sa;
    socklen_t len;

    while (!stop_task) {
        pthread_mutex_lock(epoll_mutex);
        if (stop_task) {
            pthread_mutex_unlock(epoll_mutex);
            break;
        }

        if ((*event_num) <= 0) {// epoll_events no data,get
            *event_num = epoll_wait(epoll_fd, es, max_conn, -1);
        }
        if ((*event_num)-- < 0) { // get to the end,continue
            pthread_mutex_unlock(epoll_mutex);
            if (errno == EINTR) {
                continue;
            }
            else {
                break;
            }
        }
        // get one event from epoll_events
        fd = es[(*event_num)].data.fd;
        if (fd == listen_fd) { // listen fd
            del_input_fd(fd, epoll_fd);
            pthread_mutex_unlock(epoll_mutex);
            while ((new_fd = accept(fd, NULL, NULL)) >= 0) {
                pthread_mutex_lock(&conn_mutex);
                conn_num++;
                tmp_conn = conn_num;
                pthread_mutex_unlock(&conn_mutex);
                len = sizeof(sa);
                getpeername(new_fd, (struct sockaddr *)&sa, &len);
                set_socket(new_fd, O_NONBLOCK);
                add_input_fd(new_fd, epoll_fds[incoming_fd_cnt % epoll_cnt]);
                _INFO("new conn on %s:%d,fd=%d,conn_num=%d,epollno=%d", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), new_fd, tmp_conn, incoming_fd_cnt % epoll_cnt);
                incoming_fd_cnt++;
            }
            add_input_fd(fd, epoll_fd);
        }
        else if (fd == pipe_read_fds[local_epoll_idx]) {
            _INFO("get a stop signal");
            pthread_mutex_unlock(epoll_mutex); 
            continue;
        }
        else {
            del_input_fd(fd, epoll_fd);
            pthread_mutex_unlock(epoll_mutex);
            http_handle_args handle_args;
            handle_args.fd=fd;
            handle_args.local_epoll_idx=local_epoll_idx;
            handler->handle(handle_args);
            _INFO("process");
        }

    }
    free(recv_buf);
    free(req_buf);
    free(decompress_buf);
    delete handler;
    return 0;
}


int http_acceptor::create_listen(int &socket_fd, unsigned short port) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        return -1;
    if (set_socket(socket_fd, O_NONBLOCK))
        return -1;
    if (::bind(socket_fd, (const sockaddr*)&addr, sizeof addr))
        return -1;
    if (listen(socket_fd, max_conn))
        return -1;

    return 0;
}

int http_acceptor::set_socket(int fd, int flag) {
    int options;
    options = SOCKET_SND_BUF_SIZE;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &options, sizeof(int));
    options = SOCKET_RCV_BUF_SIZE;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &options, sizeof(int));
    options = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &options, sizeof(int));
    options = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, options | flag);
    int on = 1;
    int ret = -1;
    ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));
    return ret;
}

int http_acceptor::add_input_fd(int fd, int efd) {
    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    int ret = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
    if (ret < 0) {
        _ERROR("add_input_fd err,fd=%d,errno=%d", fd, errno);
    }
    return ret;
}

int http_acceptor::del_input_fd(int fd, int efd) {
    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    int ret = epoll_ctl(efd, EPOLL_CTL_DEL, fd, &event);
    if (ret < 0) {
        _ERROR("del_input_fd err,fd=%d,errno=%d", fd, errno);
    }
    return ret;
}



uint32_t http_acceptor::genWid() {
    
    return __sync_fetch_and_add(&wid, add_sum);
}

int http_acceptor::create_pipe(int idx) {
    int options;
    int pipe_fd[2];
    
    if (pipe(pipe_fd)) {
        return -1;
    }

    pipe_read_fds[idx] = pipe_fd[0];
    pipe_write_fds[idx] = pipe_fd[1];

    for (int i = 0;i < 2;i++) {
        if ((options = fcntl(pipe_fd[i], F_GETFL)) == -1) {
            return -1;
        }
        if (fcntl(pipe_fd[i], F_SETFL, options | O_NONBLOCK) == -1) {
            return -1;
        }
    }
    return 0;
}
