#include <iostream>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "thread_pool.hpp"

#define MAX_EVENTS 10000
#define READ_BUFFER_SIZE 4096

struct HTTPResponse {
    struct iovec parts[2];
    const char* headers = "HTTP/1.1 200 OK\r\n"
                         "Content-Length: 13\r\n"
                         "Content-Type: text/plain\r\n"
                         "Connection: keep-alive\r\n\r\n";
    const char* body = "Hello, World!\n";
} static response;

extern "C" {
    void* http_server_init();
    int http_server_listen(void* server, int port);
    void http_server_stop(void* server);
}

static void* g_server = nullptr;
static int epoll_fd = -1;
static ThreadPool thread_pool;

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void optimize_socket(int sock) {
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int rcvbuf = 1024 * 1024;
    int sndbuf = 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    set_nonblocking(sock);
}

void handle_client(int client_fd) {
    char buffer[READ_BUFFER_SIZE];
    
    if (recv(client_fd, buffer, READ_BUFFER_SIZE, MSG_DONTWAIT) < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close(client_fd);
            return;
        }
    }

    struct msghdr msg = {0};
    msg.msg_iov = response.parts;
    msg.msg_iovlen = 2;
    
    ssize_t sent = sendmsg(client_fd, &msg, MSG_NOSIGNAL);
    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        close(client_fd);
    }
}

void signal_handler(int sig) {
    if (g_server) {
        std::cout << "\nStopping server..." << std::endl;
        close(epoll_fd);
        http_server_stop(g_server);
        exit(0);
    }
}

int main(int argc, char** argv) {
    response.parts[0].iov_base = (void*)response.headers;
    response.parts[0].iov_len = strlen(response.headers);
    response.parts[1].iov_base = (void*)response.body;
    response.parts[1].iov_len = strlen(response.body);

    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    g_server = http_server_init();
    if (!g_server) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    int server_fd = reinterpret_cast<intptr_t>(g_server);
    optimize_socket(server_fd);
    
    int port = argc > 1 ? std::atoi(argv[1]) : 3080;
    std::cout << "Starting server on port " << port << std::endl;
    
    if (http_server_listen(g_server, port) < 0) {
        std::cerr << "Failed to start server: " << strerror(errno) << std::endl;
        http_server_stop(g_server);
        return 1;
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "Failed to create epoll instance" << std::endl;
        return 1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        std::cerr << "Failed to add server socket to epoll" << std::endl;
        return 1;
    }

    struct epoll_event events[MAX_EVENTS];
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == server_fd) {
                for (int i = 0; i < 100; i++) {
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_fd = accept4(server_fd,
                        reinterpret_cast<struct sockaddr*>(&client_addr),
                        &addr_len, SOCK_NONBLOCK);

                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        continue;
                    }

                    optimize_socket(client_fd);

                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
                    client_ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
                }
            } else {
                if (events[n].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    close(events[n].data.fd);
                    continue;
                }
                
                thread_pool.enqueue([fd = events[n].data.fd]() {
                    handle_client(fd);
                });
            }
        }
    }
    
    return 0;
}