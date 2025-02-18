#include "socket.h"
#include "http/types.h"
#include "http/router.h"
#include "http/parser.h"
#include "config/config.h"
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstring>
#include <string>
#include <memory>
#include <atomic>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

template<typename T>
class SafeQueue {
private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable cond;
public:
    bool push(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(value));
        cond.notify_one();
        return true; 
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return false;
        value = std::move(queue.front());
        queue.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
};

class ConnectionPool {
private:
    static constexpr size_t POOL_SIZE = 50000;
    std::vector<std::unique_ptr<RawSocket>> pool;
    std::mutex mtx;
    std::condition_variable cv;
    size_t available;

public:
    ConnectionPool() : available(POOL_SIZE) {
        pool.reserve(POOL_SIZE);
    }

    std::unique_ptr<RawSocket> acquire() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return available > 0; });
        available--;
        auto socket = std::make_unique<RawSocket>();
        socket->create();
        socket->enable_zero_copy();
        return socket;
    }

    void release(std::unique_ptr<RawSocket> socket) {
        std::lock_guard<std::mutex> lock(mtx);
        socket->close();
        available++;
        cv.notify_one();
    }
};

class BufferPool {
public:
    static constexpr size_t BUFFER_SIZE = 16384;
private:
    SafeQueue<char*> pool;
    std::atomic<size_t> allocated{0};

public:
    char* acquire() {
        char* buffer;
        if (!pool.try_pop(buffer)) {
            allocated++;
            return new char[BUFFER_SIZE];
        }
        return buffer;
    }
    
    void release(char* buffer) {
        if (!pool.push(buffer)) {
            delete[] buffer;
            allocated--;
        }
    }

    ~BufferPool() {
        char* buffer;
        while (pool.try_pop(buffer)) delete[] buffer;
    }
};

class ThreadPool {
private:
    std::vector<std::thread> workers;
    SafeQueue<std::function<void()>> tasks;
    std::atomic<bool> stop{false};
    std::atomic<size_t> active_threads{0};
    std::mutex cv_mutex;
    std::condition_variable cv;
    size_t num_threads;
    static constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(100);

public:
    ThreadPool(size_t threads) : num_threads(threads) {
        for(size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                std::function<void()> task;
                while (!stop) {
                    {
                        std::unique_lock<std::mutex> lock(cv_mutex);
                        if (cv.wait_for(lock, IDLE_TIMEOUT, [this] { 
                            return !tasks.empty() || stop; 
                        })) {
                            if (!tasks.try_pop(task)) continue;
                        } else {
                            continue;
                        }
                    }
                    active_threads++;
                    task();
                    active_threads--;
                }
            });
        }
    }

    void enqueue(std::function<void()>&& f) {
        tasks.push(std::move(f));
        cv.notify_one();
    }

    size_t thread_count() const { return num_threads; }

    size_t active_count() const { return active_threads; }

    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        for(auto &worker : workers) {
            worker.join();
        }
    }
};

std::string build_response(const http::Response& res) {
    size_t total_size = 128;
    for (const auto& [key, value] : res.headers) {
        total_size += key.size() + value.size() + 4;
    }
    total_size += res.body.size();
    
    std::string response;
    response.reserve(total_size);
    
    static const std::string_view HTTP_OK = "HTTP/1.1 200 OK\r\n";
    static const std::string_view HTTP_404 = "HTTP/1.1 404 Not Found\r\n";
    static const std::string_view HTTP_500 = "HTTP/1.1 500 Internal Server Error\r\n";
    
    switch (res.status) {
        case http::Status::OK: response.append(HTTP_OK); break;
        case http::Status::NotFound: response.append(HTTP_404); break;
        case http::Status::InternalError: response.append(HTTP_500); break;
        default: response.append(HTTP_OK);
    }

    for (const auto& [key, value] : res.headers) {
        response += key + ": " + value + "\r\n";
    }
    response += "\r\n";

    response.insert(response.end(), res.body.begin(), res.body.end());
    return response;
}

class HTTPServer {
private:
    static constexpr int DEFAULT_MAX_THREADS = 10240;
    const int MAX_THREADS;
    const int BACKLOG_SIZE;
    const int MAX_CONNECTIONS;
    const size_t BATCH_SIZE;
    const std::string HOST;
    
    ConnectionPool conn_pool;
    BufferPool buffer_pool;
    ThreadPool thread_pool;
    RawSocket server_socket;
    std::string_view response;
    std::atomic<size_t> active_connections{0};
    std::atomic<uint64_t> request_count{0};
    std::atomic<uint64_t> error_count{0};
    std::atomic<bool> running{true};
    std::condition_variable accept_cv;
    std::mutex accept_mutex;
    http::Router router;

    static size_t get_optimal_threads() {
        size_t cpu_cores = 1;
        
        #if defined(_WIN32)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        cpu_cores = sysInfo.dwNumberOfProcessors;
        #elif defined(__APPLE__)
        int nm[2];
        size_t len = 4;
        nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
        sysctl(nm, 2, &cpu_cores, &len, NULL, 0);
        #elif defined(__linux__)
        cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpu_cores < 1) cpu_cores = 1;
        #endif
        
        size_t max_threads = Config::instance().get_int("max_threads", 10240);
        return std::min(max_threads, cpu_cores + (cpu_cores >> 1));
    }

    void adaptive_sleep() {
        auto active = active_connections.load();
        if (active == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else if (thread_pool.active_count() < thread_pool.thread_count() / 2) {
            std::this_thread::yield();
        }
    }

    void handle_client(std::shared_ptr<RawSocket> client) {
        char* buffer = buffer_pool.acquire();
        size_t idle_count = 0;
        const size_t MAX_IDLE = 3;
        
        client->set_timeouts(100, 100);
        client->enable_zero_copy();
        
        while (client->is_valid() && running) {
            int bytes_received = client->recv(buffer, BufferPool::BUFFER_SIZE - 1);
            if (bytes_received <= 0) {
                if (++idle_count >= MAX_IDLE) break;
                adaptive_sleep();
                continue;
            }
            
            buffer[bytes_received] = '\0';
            
            http::Request req;
            http::Response res;
            
            auto parse_result = http::parse_request(buffer, bytes_received, req);
            if (parse_result.success) {
                if (router.handle(req, res)) {
                    if (res.headers.find("Content-Type") == res.headers.end()) {
                        res.headers["Content-Type"] = "application/json";
                    }
                    res.headers["Connection"] = "keep-alive";
                    
                    std::string response_str = build_response(res);
                    if (client->send(response_str.c_str(), response_str.length()) <= 0) {
                        error_count++;
                        break;
                    }
                } else {
                    res.status = http::Status::NotFound;
                    res.json("{\"error\":\"Not Found\"}");
                    std::string response_str = build_response(res);
                    if (client->send(response_str.c_str(), response_str.length()) <= 0) {
                        error_count++;
                        break;
                    }
                }
                request_count++;
                idle_count = 0;
            } else {
                error_count++;
                break;
            }
        }

        buffer_pool.release(buffer);
        client->close();
        active_connections--;
    }

public:
    HTTPServer() : 
        MAX_THREADS(Config::instance().get_int("max_threads", DEFAULT_MAX_THREADS)),
        BACKLOG_SIZE(Config::instance().get_int("backlog_size", 65535)),
        MAX_CONNECTIONS(Config::instance().get_int("max_connections", 9000000)),
        BATCH_SIZE(64),
        HOST(Config::instance().get_string("host", "0.0.0.0")),
        thread_pool(get_optimal_threads())
    {
        RawSocket::configure_system_limits();
        response = "HTTP/1.1 200 OK\r\n"
                  "Content-Length: 2\r\n"
                  "Connection: keep-alive\r\n"
                  "\r\n"
                  "OK";

        std::cout << "Server initialized with " << thread_pool.thread_count() 
                  << " threads (CPU cores: " << std::thread::hardware_concurrency() << ")\n";
    }

    bool start(uint16_t port = 0) {
        if (port == 0) {
            port = static_cast<uint16_t>(Config::instance().get_int("port", 8080));
        }

        if (!server_socket.create()) {
            std::cerr << "Failed to create socket\n";
            return false;
        }

        int opt = 1;
        server_socket.set_option(SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        server_socket.set_option(IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));
        
        #ifdef _WIN32
        int bufsize = 65536;
        server_socket.set_option(SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, sizeof(bufsize));
        server_socket.set_option(SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize));
        #endif

        if (!server_socket.bind(port)) {
            std::cerr << "Failed to bind to port " << port << "\n";
            return false;
        }

        if (!server_socket.listen(BACKLOG_SIZE)) {
            std::cerr << "Failed to listen on port " << port << "\n";
            return false;
        }

        std::cout << "Server running with " << thread_pool.thread_count() 
                  << " threads, max " << MAX_CONNECTIONS << " connections\n";

        std::thread stats_thread([this]() {
            uint64_t last_count = 0;
            while (running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                uint64_t current = request_count.load();
                uint64_t rps = current - last_count;
                std::cout << "RPS: " << rps << ", Active: " << active_connections 
                         << ", Errors: " << error_count << "\n";
                last_count = current;
            }
        });
        stats_thread.detach();

        if (Config::instance().get_bool("cors_enabled", true)) {
            router.use([](http::Request& req, http::Response& res) {
                res.headers["Access-Control-Allow-Origin"] = 
                    Config::instance().get_string("cors_allow_origin", "*");
                return true;
            });
        }

        std::vector<std::thread> accept_threads;
        const size_t NUM_ACCEPT_THREADS = std::min(size_t(16), thread_pool.thread_count() / 4);

        for (size_t i = 0; i < NUM_ACCEPT_THREADS; ++i) {
            accept_threads.emplace_back([this] {
                while (running) {
                    if (active_connections >= MAX_CONNECTIONS) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }

                    RawSocket client_socket = server_socket.accept();
                    if (client_socket.is_valid()) {
                        active_connections++;
                        auto client = std::make_shared<RawSocket>(std::move(client_socket));
                        thread_pool.enqueue([this, client] {
                            handle_client(client);
                        });
                    } else {
                        adaptive_sleep();
                    }
                }
            });
        }

        for (auto& thread : accept_threads) {
            thread.join();
        }

        return true;
    }

    void get(const std::string& path, http::RequestHandler handler) {
        router.get(path, handler);
    }

    void post(const std::string& path, http::RequestHandler handler) {
        router.post(path, handler);
    }

    void use(http::Middleware middleware) {
        router.use(middleware);
    }

    ~HTTPServer() {
        running = false;
        accept_cv.notify_all();
    }
};

int main(int argc, char* argv[]) {
    bool config_loaded = Config::instance().load("config.json");
    if (!config_loaded) {
        std::cerr << "Failed to load config\n";

        std::cerr << "Using default values\n";
    }

    if (!RawSocket::initialize()) {
        std::cerr << "Failed to initialize sockets\n";
        return 1;
    }

    uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
        if (port == 0) {
            std::cerr << "Invalid port number\n";
            return 1;
        }
    }

    HTTPServer server;

    server.use([](http::Request& req, http::Response& res) {
        std::cout << req.method << " " << req.url << "\n";
        return true;
    });

    server.get("/", [](http::Request& req, http::Response& res) {
        res.json("{\"status\":\"ok\"}");
    });

    server.get("/users/:id", [](http::Request& req, http::Response& res) {
        auto id = req.params["id"];
        res.json("{\"id\":\"" + id + "\"}");
    });

    std::cout << "Starting server on " 
              << Config::instance().get_string("host", "0.0.0.0") 
              << ":" << Config::instance().get_int("port", 8080) << "...\n";
              
    bool result = server.start();
    
    RawSocket::cleanup();
    return result ? 0 : 1;
}
