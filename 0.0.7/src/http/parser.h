#pragma once
#include "types.h"
#include <string>
#include <cctype>

namespace http {

struct ParseResult {
    bool success;
    size_t bytes_consumed;
    size_t max_length = 8192;
};

ParseResult parse_request(const char* buffer, size_t length, Request& req) {
    ParseResult result{false, 0};
    if (length > result.max_length) {
        return result;
    }
    
    if (!buffer || length == 0) {
        return result;
    }
    
    std::string request(buffer, length);
    size_t pos = 0;
    size_t end_line = request.find("\r\n");

    if (end_line == std::string::npos) return result;

    std::string method_str;
    size_t space = request.find(' ');
    if (space == std::string::npos || space > end_line) return result;
    
    method_str = request.substr(0, space);
    if (method_str == "GET") req.method = Method::GET;
    else if (method_str == "POST") req.method = Method::POST;
    else if (method_str == "PUT") req.method = Method::PUT;
    else if (method_str == "DELETE") req.method = Method::DEL;
    else if (method_str == "HEAD") req.method = Method::HEAD;
    else if (method_str == "OPTIONS") req.method = Method::OPTIONS;
    else if (method_str == "PATCH") req.method = Method::PATCH;
    else return result;

    pos = space + 1;
    while (pos < end_line && std::isspace(request[pos])) pos++;
    
    space = request.find(' ', pos);
    if (space == std::string::npos || space > end_line) return result;
    req.url = request.substr(pos, space - pos);

    pos = space + 1;
    while (pos < end_line && std::isspace(request[pos])) pos++;
    req.version = request.substr(pos, end_line - pos);

    pos = end_line + 2;
    while (pos < length) {
        end_line = request.find("\r\n", pos);
        if (end_line == std::string::npos) break;
        if (pos == end_line) {
            pos = end_line + 2;
            break;
        }

        std::string line = request.substr(pos, end_line - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            
            while (!value.empty() && std::isspace(value[0])) 
                value.erase(0, 1);
            while (!value.empty() && std::isspace(value.back())) 
                value.pop_back();
                
            req.headers[key] = value;
        }
        pos = end_line + 2;
    }

    if (pos < length) {
        req.body.assign(buffer + pos, buffer + length);
    }

    result.success = true;
    result.bytes_consumed = pos;
    return result;
}

}