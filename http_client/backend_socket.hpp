#ifndef BACKEND_SOCKET_HPP
#define BACKEND_SOCKET_HPP

#include <pthread.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "service_log.hpp"

#define BK_SOCKET_SND_BUF_SIZE (50*1024)
#define BK_SOCKET_RCV_BUF_SIZE (50*1024)

typedef enum {
    VALID,
    INVALID
} socket_status;

class backend_socket {
    private:
        int fd;
        
        sockaddr_in* address;
        sockaddr_in real_address;

        std::string sket_id;        
        std::string addr_str;
        
        socket_status status;
        
        pthread_mutex_t _mutex;
        pthread_rwlock_t _stat_lock;

    public:
        backend_socket() {
            fd = -1;
            address = NULL;
            status = INVALID;
            pthread_mutex_init(&_mutex, NULL);
            pthread_rwlock_init(&_stat_lock, NULL);
        }

        ~backend_socket() {
            pthread_mutex_destroy(&_mutex);
            pthread_rwlock_destroy(&_stat_lock);
        }

        int open(sockaddr_in* _address) {
            real_address = *_address;
            address = _address;
            char addr_buf[128];
            snprintf(addr_buf, 128, "%s:%d", inet_ntoa(address->sin_addr), ntohs(address->sin_port));
            addr_str = std::string(addr_buf);
            sket_id = "-1@" + addr_str;
        }

        int close_fd() {
            int ret = close(fd);
            if (ret < 0) {
                _ERROR("try to close fd %d error, errno=%d", fd, errno);
                perror("serious error");
            }
            return ret;
        }
        int close_fd_onfail() {
            int ret = close(fd);
            if (ret < 0) {
                _ERROR("try to close fd %d error, errno=%d", fd, errno);
                perror("close fd onfail error");
            }
            return ret;
        }

        int connectSocket(int timeout) {
            if (address == NULL) {
                _ERROR("null address");
                status = INVALID;
                return -1;
            }
            //_INFO("begin to connect to %s:%d", inet_ntoa(address->sin_addr), ntohs(address->sin_port));
            if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                _ERROR("create socket fd error on %s:%d", inet_ntoa(address->sin_addr), ntohs(address->sin_port));
                status = INVALID;
                return -1;
            }
            int options;
            options = fcntl(fd, F_GETFL);
            fcntl(fd, F_SETFL, options | O_NONBLOCK);
            timeval begin;
            gettimeofday(&begin, NULL);
            if (connect(fd, (struct sockaddr*)address, sizeof(*address)) < 0) {
                if (errno == EINPROGRESS) {
                    pollfd connect_fd;
                    connect_fd.fd = fd;
                    connect_fd.events = POLLOUT;
                    int poll_ret = poll(&connect_fd, 1, timeout); // timeout unit is ms
                    timeval end;
                    gettimeofday(&end, NULL);
                    _INFO("connect cost=%ld", end.tv_sec * 1000000 + end.tv_usec - begin.tv_sec * 1000000 - begin.tv_usec);
                    if (poll_ret <= 0 || !(connect_fd.revents & POLLOUT)) {
                        _ERROR("connect socket fail on %s:%d", inet_ntoa(address->sin_addr), ntohs(address->sin_port));
                        status = INVALID;
                        return -1;
                    }
                    int opt;
                    socklen_t  olen = sizeof(opt);
                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &opt, &olen) != 0) {
                        _ERROR("get new fd opt err");
                        status = INVALID;
                        return -1;
                    }
                    if (opt != 0) {
                        _ERROR("connect socket fail2 on %s:%d, err=%d", inet_ntoa(address->sin_addr), ntohs(address->sin_port), opt);
                        status = INVALID;
                        return -1;
                    }
                }
                else {
                    status = INVALID;
                    return -1;
                }
            }
            _INFO("connect to %s:%d success", inet_ntoa(address->sin_addr), ntohs(address->sin_port));
            status = VALID;
            setSocket();
            return fd;
        }

        int setSocket() {
            if (fd < 0) {
                _ERROR("error socket fd=%d", fd);
                return -1;
            }
            int options;
            options = BK_SOCKET_SND_BUF_SIZE;
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &options, sizeof(int));
            options = BK_SOCKET_RCV_BUF_SIZE;
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &options, sizeof(int));
            options = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &options, sizeof(int));
            options = fcntl(fd, F_GETFL);
            fcntl(fd, F_SETFL, options | O_NONBLOCK);
            int on = 1;
            int ret = -1;
            ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));
            return ret;
        }

        int get_fd() {
            return fd;
        }

        int lock() {
            pthread_mutex_lock(&_mutex);
        }

        int unlock() {
            pthread_mutex_unlock(&_mutex);
        }

        int changeState_nolock(socket_status new_status) {
            status = new_status;
        }

        int changeState(socket_status new_status) {
            pthread_rwlock_wrlock(&_stat_lock);
            status = new_status;
            pthread_rwlock_unlock(&_stat_lock);
        }

        int getState_nolock() {
            return status;
        }

        int getState() {
            socket_status _st;
            pthread_rwlock_rdlock(&_stat_lock);
            _st = status;
            pthread_rwlock_unlock(&_stat_lock);
            return _st;
        }
    
};

#endif

