#pragma once

#include <string>
#include <vector>
#include <memory>
#include "nx/common.hpp"

namespace nx::util {

struct HttpResponse {
    int status_code = 0;
    std::string body;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    
    // Non-copyable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    
    // Movable
    HttpClient(HttpClient&&) = default;
    HttpClient& operator=(HttpClient&&) = default;
    
    Result<HttpResponse> post(const std::string& url, 
                             const std::string& body,
                             const std::vector<std::string>& headers = {});

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace nx::util