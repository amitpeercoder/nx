#include "nx/util/http_client.hpp"

#include <curl/curl.h>
#include <sstream>
#include <stdexcept>

namespace nx::util {

struct HttpClient::Impl {
    CURL* curl = nullptr;
    
    Impl() {
        curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }
    
    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

HttpClient::HttpClient() : pImpl(std::make_unique<Impl>()) {
}

HttpClient::~HttpClient() = default;

Result<HttpResponse> HttpClient::post(const std::string& url, 
                                     const std::string& body,
                                     const std::vector<std::string>& headers) {
    if (!pImpl->curl) {
        return std::unexpected(makeError(ErrorCode::kNetworkError, "CURL not initialized"));
    }
    
    std::string response_body;
    long response_code = 0;
    
    // Reset curl handle
    curl_easy_reset(pImpl->curl);
    
    // Set URL
    curl_easy_setopt(pImpl->curl, CURLOPT_URL, url.c_str());
    
    // Set POST
    curl_easy_setopt(pImpl->curl, CURLOPT_POST, 1L);
    
    // Set body
    curl_easy_setopt(pImpl->curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(pImpl->curl, CURLOPT_POSTFIELDSIZE, body.length());
    
    // Set headers
    struct curl_slist* header_list = nullptr;
    for (const auto& header : headers) {
        header_list = curl_slist_append(header_list, header.c_str());
    }
    if (header_list) {
        curl_easy_setopt(pImpl->curl, CURLOPT_HTTPHEADER, header_list);
    }
    
    // Set callback for response body
    curl_easy_setopt(pImpl->curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(pImpl->curl, CURLOPT_WRITEDATA, &response_body);
    
    // Set timeout
    curl_easy_setopt(pImpl->curl, CURLOPT_TIMEOUT, 60L);
    
    // Perform the request
    CURLcode res = curl_easy_perform(pImpl->curl);
    
    // Clean up headers
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    
    if (res != CURLE_OK) {
        return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                       "HTTP request failed: " + std::string(curl_easy_strerror(res))));
    }
    
    // Get response code
    curl_easy_getinfo(pImpl->curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    HttpResponse response;
    response.status_code = static_cast<int>(response_code);
    response.body = response_body;
    
    return response;
}

} // namespace nx::util