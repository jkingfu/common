#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <locale.h>
#include <malloc.h>
#include <service_log.hpp>
#include <unistd.h>

#include "configuration.hpp"
#include "http_acceptor.hpp"


using namespace std;

void usage(const char *bin_name) {
    printf("Usage:\n");
    printf("        %s [options]\n\n", bin_name);
    printf("Options:\n");
    printf("        -h:             Show help messages.\n");
}

void sigterm_handler(int signo) {}
void sigint_handler(int signo) {}

int main(int argc, char* argv[]) {
    mallopt(M_MMAP_THRESHOLD, 256*1024);
    setenv("LC_CTYPE", "zh_CN.gbk", 1);

    int ret;
    close(STDIN_FILENO);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, &sigterm_handler);
    signal(SIGINT, &sigint_handler);

    configuration config;
    config.port=8080;
    config.max_conn=1000;
    config.handler_thread_num=30;
    config.thread_stack_size=2048 * (1 << 10);
    config.epoll_cnt=4;
    http_acceptor* acceptor;

    _INFO("http starting");

    acceptor = new http_acceptor(config.epoll_cnt, config.max_conn);
    ret = acceptor->open(config);
    if (ret < 0) {
        _ERROR("acceptor open fail");
        exit(-1);
    }
    _INFO("open acceptor ok");

    acceptor->activate();
    _INFO("acceptor svc ok");

    _INFO("http start ok");
    pause();
    _INFO("http is stopping");

    acceptor->stop();
    delete acceptor;

    _INFO("all stop ok");

    return 0;
}
