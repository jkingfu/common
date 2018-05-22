#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
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

    hostent *svr_host = gethostbyname(g_svr_addr[i].c_str());
    if (svr_host == NULL) {
        _ERROR("[Hint] Get host %s:%d by name failed: %s\n", g_svr_addr[i].c_str(),g_svr_port[i], strerror(errno));
        return;
    }
    sockaddr_in svr;
    bzero(&svr, sizeof(svr));
    svr.sin_family = AF_INET;
    svr.sin_addr = *((in_addr *) (svr_host->h_addr));
    svr.sin_port = htons(g_svr_port[i]);
    
    backend_socket*  socket = new backend_socket();

    while (!stop_task) {    
        socket.open(&svr);
        int ret =socket.connectSocket(tcp_connect_timeout);
        if (ret < 0) {
            socket.close_fd_onfail();
        } else {
            socket.close_fd();
        }
    }
}


