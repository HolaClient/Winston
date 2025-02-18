#include "socket.h"
#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#endif

bool RawSocket::init() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

void RawSocket::clean() {
#ifdef _WIN32
    WSACleanup();
#endif
}

RawSocket::RawSocket() : socket_handle(INVALID_SOCKET) {}

RawSocket::~RawSocket() {
    if (valid()) {
        shut(SD_BOTH);
        closesocket(socket_handle);
    }
}

bool RawSocket::make() {
    socket_handle = socket(AF_INET, SOCK_STREAM, 0);
    if (valid()) {
        int opt = 1;
        opt(SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        opt(IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
    }
    return valid();
}

bool RawSocket::bind(uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    addr.sin_port = htons(port);
    
    if (::bind(socket_handle, (sockaddr*)&addr, sizeof(addr)) < 0) {
        #ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAEADDRINUSE) {
            std::cerr << "Port " << port << " is already in use\n";
        }
        #else
        if (errno == EADDRINUSE) {
            std::cerr << "Port " << port << " is already in use\n";
        }
        #endif
        return false;
    }
    return true;
}

bool RawSocket::listen(int backlog) {
    return ::listen(socket_handle, 128) == 0;
}

RawSocket RawSocket::accept() {
    RawSocket client;
    sockaddr_in addr{};
    #ifdef _WIN32
    int len = sizeof(addr);
    #else
    socklen_t len = sizeof(addr);
    #endif
    
    client.socket_handle = ::accept(socket_handle, (sockaddr*)&addr, &len);
    
    if (client.is_valid()) {
        int opt = 1;
        client.set_option(IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
        #ifdef TCP_QUICKACK
        client.set_option(IPPROTO_TCP, TCP_QUICKACK, (char*)&opt, sizeof(opt));
        #endif
        client.set_blocking(true);
    }
    
    return client;
}

int RawSocket::send(const char* data, int length) {
    return ::send(socket_handle, data, length, 0);
}

int RawSocket::recv(char* buffer, int length) {
    return ::recv(socket_handle, buffer, length, 0);
}

bool RawSocket::connect(const char* ip, uint16_t port, int timeout_sec) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) return false;

    blocking(false);
    
    int result = ::connect(socket_handle, (sockaddr*)&addr, sizeof(addr));
    if (result == -1) {
        #ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) return false;
        #else
        if (errno != EINPROGRESS) return false;
        #endif

        fd_set write_set;
        FD_ZERO(&write_set);
        FD_SET(socket_handle, &write_set);

        timeval timeout = {timeout_sec, 0};
        result = select(socket_handle + 1, nullptr, &write_set, nullptr, &timeout);
        
        if (result <= 0) return false;
    }

    blocking(true);
    return true;
}

void RawSocket::blocking(bool block) {
    #ifdef _WIN32
    u_long mode = block ? 0 : 1;
    ioctlsocket(socket_handle, FIONBIO, &mode);
    #else
    int flags = fcntl(socket_handle, F_GETFL, 0);
    fcntl(socket_handle, F_SETFL, block ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
    #endif
}

int RawSocket::error() const {
    #ifdef _WIN32
    return WSAGetLastError();
    #else
    return errno;
    #endif
}

bool RawSocket::set_timeouts(int send_timeout_ms, int recv_timeout_ms) {
    #ifdef _WIN32
    DWORD timeout = send_timeout_ms;
    if (setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
        return false;
    }
    timeout = recv_timeout_ms;
    if (setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
        return false;
    }
    #else
    struct timeval tv;
    tv.tv_sec = send_timeout_ms / 1000;
    tv.tv_usec = (send_timeout_ms % 1000) * 1000;
    if (setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
        return false;
    }
    tv.tv_sec = recv_timeout_ms / 1000;
    tv.tv_usec = (recv_timeout_ms % 1000) * 1000;
    if (setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
        return false;
    }
    #endif
    return true;
}

RawSocket::RawSocket(RawSocket&& other) noexcept : socket_handle(other.socket_handle) {
    other.socket_handle = INVALID_SOCKET;
}

RawSocket& RawSocket::operator=(RawSocket&& other) noexcept {
    if (this != &other) {
        if (valid()) {
            shut(SD_BOTH);
            closesocket(socket_handle);
        }
        socket_handle = other.socket_handle;
        other.socket_handle = INVALID_SOCKET;
    }
    return *this;
}
