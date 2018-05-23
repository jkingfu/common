#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <locale.h>
#include <malloc.h>
#include <service_log.hpp>
#include <unistd.h>

#include "configuration.hpp"
#include "http_client.hpp"


using namespace std;

void usage(const char *bin_name) {
    printf("Usage:\n");
    printf("        %s [options]\n\n", bin_name);
    printf("Options:\n");
    printf("        -d:             domain.\n");
    printf("        -p:             port.\n");
    printf("        -t:             tcp_connect_timeout.\n");
    printf("        -n:             handler_thread_num.\n");
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
    
    char opt_char;
    while ((opt_char = getopt(argc, argv, "d:p:t:n:h")) != -1) {
        switch (opt_char) {
            case 'd':
                config.domain = optarg;
                break;
            case 'p':
                config.port = atoi(optarg);
                break;
            case 't':
                config.tcp_connect_timeout = atoi(optarg);
                break;
            case 'n':
                config.handler_thread_num = atoi(optarg);
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                break;
        }
    }
    
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
