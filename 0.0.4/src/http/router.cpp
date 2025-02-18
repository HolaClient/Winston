#include "router.h"
#include <sstream>
#include <regex>

namespace http {

void Router::add_route(Method method, const std::string& path, RequestHandler handler) {
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

bool Router::match_route(const Route& route, const std::string& path, Request& req) {
    std::smatch matches;
    if (std::regex_match(path, matches, route.pattern)) {
        for (size_t i = 1; i < matches.size() && i-1 < route.paramNames.size(); ++i) {
            req.params[route.paramNames[i-1]] = matches[i].str();
        }
        return true;
    }
    return false;
}

}
