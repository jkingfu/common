#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <service_log.hpp>
#include "http_client.hpp"
#include "backend_socket.hpp"



http_client::~http_client() {

}


int http_client::open(const configuration& config_) {
    config = &config_;
    _INFO("start thread num=%d", config_.handler_thread_num);
    return task_base::open(config_.handler_thread_num, config_.thread_stack_size);
}

int http_client::stop() {
    stop_task = 1;
    join();
    _INFO("http_client stops");
    return 0;
}


int http_client::svc() {

    hostent *svr_host = gethostbyname(config->ip.c_str());
    if (svr_host == NULL) {
        _ERROR("[Hint] Get host %s by name failed: %s\n", config->ip.c_str(), strerror(errno));
        return 1;
    }
    sockaddr_in svr;
    bzero(&svr, sizeof(svr));
    svr.sin_family = AF_INET;
    svr.sin_addr = *((in_addr *) (svr_host->h_addr));
    svr.sin_port = htons(config->port);
    
    backend_socket*  socket = new backend_socket();
    socket->open(&svr);

    while (!stop_task) {    
        int ret =socket->connectSocket(config->tcp_connect_timeout);
        if (ret < 0) {
            socket->close_fd_onfail();
        } else {
            socket->close_fd();
        }
    }
    delete socket;
    return 0;
}


