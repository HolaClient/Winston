#pragma once

#ifdef DELETE
#undef DELETE
#endif

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <ostream>

namespace http {

enum class Method { 
    GET, 
    POST, 
    PUT, 
    DEL,
    HEAD, 
    OPTIONS, 
    PATCH,
    CONNECT,
    TRACE
};

inline std::ostream& operator<<(std::ostream& os, const Method& method) {
    switch(method) {
        case Method::GET:           os << "GET"; break;
        case Method::POST:          os << "POST"; break;
        case Method::PUT:           os << "PUT"; break;
        case Method::DEL: os << "DELETE"; break;
        case Method::HEAD:          os << "HEAD"; break;
        case Method::OPTIONS:       os << "OPTIONS"; break;
        case Method::PATCH:         os << "PATCH"; break;
        case Method::CONNECT:       os << "CONNECT"; break;
        case Method::TRACE:         os << "TRACE"; break;
        default:                    os << "UNKNOWN"; break;
    }
    return os;
}

enum class Status {
    OK = 200,
    Created = 201,
    NoContent = 204,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    TooManyRequests = 429,
    InternalError = 500
};

struct Headers : std::map<std::string, std::string> {
    void set_content_type(const std::string& type) {
        (*this)["Content-Type"] = type;
    }
    void set_content_length(size_t length) {
        (*this)["Content-Length"] = std::to_string(length);
    }
};

struct Request {
    Method method;
    std::string url;
    std::string version;
    Headers headers;
    std::vector<char> body;
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> params;
};

struct Response {
    Status status{Status::OK};
    Headers headers;
    std::vector<char> body;
    bool ended{false};

    void send(const std::string& data) {
        body.assign(data.begin(), data.end());
        headers.set_content_length(data.length());
    }

    void json(const std::string& json_data) {
        headers.set_content_type("application/json");
        send(json_data);
    }

    void end() {
        ended = true;
    }
};

using RequestHandler = std::function<void(Request&, Response&)>;
using Middleware = std::function<bool(Request&, Response&)>;

}