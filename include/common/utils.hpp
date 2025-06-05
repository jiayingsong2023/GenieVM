#pragma once

#include <string>
#include <curl/curl.h>

namespace utils {

inline std::string urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return str;
    }
    
    char* encoded = curl_easy_escape(curl, str.c_str(), str.length());
    std::string result(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

} // namespace utils 