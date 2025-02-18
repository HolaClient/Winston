#pragma once
#include <cstdint>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
typedef int socket_t;
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#define SD_BOTH SHUT_RDWR
#define closesocket close
#endif

#ifdef _WIN32
#define MAX_CONN 50000
#else
#include <sys/resource.h>
#endif

class RawSocket {
public:
    static bool init();
    static void clean();
    
    RawSocket();
    ~RawSocket();
    
    bool make();
    bool bind(uint16_t port);
    bool listen(int backlog);
    RawSocket accept();
    bool connect(const char* ip, uint16_t port, int timeout_sec = 5);
    int send(const char* data, int length);
    int recv(char* buffer, int length);
    bool valid() const { return socket_handle != INVALID_SOCKET; }
    int error() const;
    void blocking(bool block);
    socket_t handle() const { return socket_handle; }
    bool set_timeouts(int send_ms, int recv_ms);
    void shut(int how) { ::shutdown(socket_handle, how); }
    void opt(int level, int name, const char* val, int len) { 
        setsockopt(socket_handle, level, name, val, len); 
    }

    RawSocket(RawSocket&& other) noexcept;
    RawSocket& operator=(RawSocket&& other) noexcept;
    
    RawSocket(const RawSocket&) = delete;
    RawSocket& operator=(const RawSocket&) = delete;

private:
    socket_t socket_handle;
};