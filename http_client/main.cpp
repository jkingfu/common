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
    config.ip="10.134.64.94";
    config.domain="10.134.64.94";
    config.tcp_connect_timeout=20;
    config.handler_thread_num=30;
    config.thread_stack_size=2048 * (1 << 10);
    
    http_client* client;

    _INFO("client starting");

    client = new http_client();
    ret = client->open(config);
    if (ret < 0) {
        _ERROR("client open fail");
        exit(-1);
    }
    _INFO("open client ok");

    client->activate();
    _INFO("client svc ok");

    _INFO("client start ok");
    pause();
    _INFO("client is stopping");

    client->stop();
    delete client;

    _INFO("all stop ok");

    return 0;
}
