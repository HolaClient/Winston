#pragma once
#include "types.h"
#include <regex>
#include <vector>
#include <unordered_map>
#include <sstream>

namespace http {

class Router {
    struct Route {
        std::regex pattern;
        RequestHandler handler;
        std::vector<std::string> paramNames;
    };

    std::vector<Middleware> middleware;
    std::unordered_map<Method, std::vector<Route>> routes;

public:
    void use(Middleware m) { 
        if (m) middleware.push_back(m); 
    }
    
    void get(const std::string& path, RequestHandler handler) {
        if (handler) add_route(Method::GET, path, handler);
    }
    
    void post(const std::string& path, RequestHandler handler) {
        if (handler) add_route(Method::POST, path, handler);
    }

    bool handle(Request& req, Response& res) {
        for (const auto& m : middleware) {
            if (!m(req, res)) return false;
        }

        auto it = routes.find(req.method);
        if (it != routes.end()) {
            for (const auto& route : it->second) {
                if (match_route(route, req.url, req)) {
                    route.handler(req, res);
                    return true;
                }
            }
        }
        
        res.status = Status::NotFound;
        return false;
    }

private:
    void add_route(Method method, const std::string& path, RequestHandler handler) {
        Route route;
        route.handler = handler;

        std::string pattern = path;
        std::string::size_type pos = 0;
        
        while ((pos = pattern.find(":", pos)) != std::string::npos) {
            auto end = pattern.find("/", pos);
            if (end == std::string::npos) end = pattern.length();
            
            std::string param_name = pattern.substr(pos + 1, end - pos - 1);
            route.paramNames.push_back(param_name);
            
            pattern.replace(pos, end - pos, "([^/]+)");
            pos += 7;
        }

        pattern = "^" + pattern + "$";
        route.pattern = std::regex(pattern);
        
        routes[method].push_back(std::move(route));
    }

    bool match_route(const Route& route, const std::string& path, Request& req) {
        std::smatch matches;
        if (std::regex_match(path, matches, route.pattern)) {
            for (size_t i = 1; i < matches.size() && i-1 < route.paramNames.size(); ++i) {
                req.params[route.paramNames[i-1]] = matches[i].str();
            }
            return true;
        }
        return false;
    }
};

}