#include "common/vsphere_rest_client.hpp"
#include "common/utils.hpp"
#include "common/logger.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <curl/urlapi.h>  // For URL encoding
#include <regex>

// Helper function to URL encode a string
std::string urlEncode(const std::string& str) {
    char* encoded = curl_easy_escape(nullptr, str.c_str(), str.length());
    if (!encoded) {
        Logger::error("Failed to URL encode string");
        return str;
    }
    std::string result(encoded);
    curl_free(encoded);
    return result;
}

// Helper function to parse STS challenge
STSChallenge parseSTSChallenge(const std::string& challenge) {
    STSChallenge result;
    std::regex realmRegex("realm=\"([^\"]+)\"");
    std::regex serviceRegex("service=\"([^\"]+)\"");
    std::regex stsRegex("sts=\"([^\"]+)\"");
    std::regex signRealmRegex("SIGN realm=([^,]+)");

    std::smatch matches;
    if (std::regex_search(challenge, matches, realmRegex)) {
        result.realm = matches[1];
    }
    if (std::regex_search(challenge, matches, serviceRegex)) {
        result.service = matches[1];
    }
    if (std::regex_search(challenge, matches, stsRegex)) {
        result.stsUrl = matches[1];
    }
    if (std::regex_search(challenge, matches, signRealmRegex)) {
        result.signRealm = matches[1];
    }

    return result;
}

// Helper function to analyze authentication errors
void analyzeAuthError(const std::string& responseData, const std::string& username) {
    Logger::error("Authentication Error Analysis:");
    
    // Check for common authentication error patterns
    if (responseData.find("invalid_grant") != std::string::npos) {
        Logger::error("Invalid credentials provided");
        Logger::error("Please verify username and password");
    }
    
    if (responseData.find("invalid_client") != std::string::npos) {
        Logger::error("Invalid client credentials");
        Logger::error("Please check if the user account is properly configured in vCenter");
    }
    
    if (responseData.find("unauthorized_client") != std::string::npos) {
        Logger::error("Client is not authorized to use this authentication method");
        Logger::error("Please check user permissions in vCenter");
    }
    
    if (responseData.find("invalid_request") != std::string::npos) {
        Logger::error("Invalid request format");
        Logger::error("This might be due to special characters in username/password");
        std::string specialChars = "Username contains special characters: ";
        specialChars += (username.find('@') != std::string::npos ? "Yes (@)" : "No");
        specialChars += (username.find('%') != std::string::npos ? " Yes (%)" : "");
        specialChars += (username.find('$') != std::string::npos ? " Yes ($)" : "");
        Logger::debug(specialChars);
    }
    
    if (responseData.find("invalid_scope") != std::string::npos) {
        Logger::error("Invalid scope requested");
        Logger::error("Please check if the user has the required permissions");
    }
    
    if (responseData.find("server_error") != std::string::npos) {
        Logger::error("vCenter server authentication error");
        Logger::error("This might be a temporary issue or server configuration problem");
    }
}

VSphereRestClient::VSphereRestClient(const std::string& host, const std::string& username, const std::string& password)
    : host_(host), username_(username), password_(password), curl_(nullptr), isLoggedIn_(false) {
    Logger::debug("Initializing VSphereRestClient for host: " + host);
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl_ = curl_easy_init();
    if (!curl_) {
        Logger::error("Failed to initialize CURL");
        throw std::runtime_error("Failed to initialize CURL");
    }

    // TODO: Implement proper SSL certificate verification
    // Current implementation disables SSL verification as a temporary workaround
    // for self-signed certificates. This should be replaced with proper certificate
    // handling in production environments.
    // Options to consider:
    // 1. Add vCenter's CA certificate to trusted store
    // 2. Use a custom CA bundle file
    // 3. Implement certificate pinning
    Logger::warning("SSL verification is disabled. This is not recommended for production use.");
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);  // Disable SSL certificate verification
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);  // Disable hostname verification

    // Set connection timeouts and keep-alive settings
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 30L);  // 30 seconds connection timeout
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 300L);        // 5 minutes operation timeout
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);    // Enable TCP keep-alive
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPIDLE, 60L);    // Keep-alive idle time
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPINTVL, 30L);   // Keep-alive interval
    
    Logger::debug("CURL options configured: SSL verification disabled (temporary), connection timeout: 30s, operation timeout: 300s");
}

VSphereRestClient::~VSphereRestClient() {
    Logger::debug("Cleaning up VSphereRestClient");
    // Don't automatically logout - let the owner handle that
    if (curl_) {
        curl_easy_cleanup(curl_);
        Logger::debug("CURL handle cleaned up");
    }
    curl_global_cleanup();
    Logger::debug("VSphereRestClient cleanup completed");
}

bool VSphereRestClient::login() {
    // Immediate debug output
    fprintf(stderr, "Starting login process...\n");
    fflush(stderr);

    // Create authorization header
    std::string auth = username_ + ":" + password_;
    std::string base64Auth = base64_encode(auth);
    std::string authHeader = "Authorization: Basic " + base64Auth;
    
    fprintf(stderr, "Auth header created, length: %zu\n", authHeader.length());
    fflush(stderr);
    
    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, authHeader.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    
    fprintf(stderr, "Headers set up\n");
    fflush(stderr);
    
    // Build URL
    std::string url = "https://" + host_ + "/rest/com/vmware/cis/session";
    
    fprintf(stderr, "Making request to: %s\n", url.c_str());
    fflush(stderr);
    
    // Set basic CURL options
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, "");
    
    Logger::debug("Basic CURL options set");
    
    // Set up verbose logging
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);
    FILE* verbose = fopen("/tmp/curl_verbose.log", "a");
    if (!verbose) {
        Logger::debug("Failed to open verbose log file");
    } else {
        curl_easy_setopt(curl_, CURLOPT_STDERR, verbose);
    }
    
    Logger::debug("Making request to: " + url);
    
    // Perform the request
    Logger::debug("Starting CURL request...");
    std::string responseData;
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseData);
    
    CURLcode res = curl_easy_perform(curl_);
    
    long httpCode = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &httpCode);
    Logger::debug("CURL request completed with code: " + std::to_string(httpCode));
    
    if (res != CURLE_OK) {
        Logger::debug("CURL error: " + std::string(curl_easy_strerror(res)));
        if (verbose) {
            fclose(verbose);
        }
        curl_slist_free_all(headers);
        return false;
    }
    
    Logger::debug("Processing response...");
    
    // Parse response
    try {
        nlohmann::json response = nlohmann::json::parse(responseData);
        if (response.contains("value")) {
            sessionId_ = response["value"];
            isLoggedIn_ = true;
            Logger::debug("Login successful! Session ID: " + sessionId_);
            if (verbose) {
                fclose(verbose);
            }
            curl_slist_free_all(headers);
            return true;
        } else {
            Logger::debug("Response missing session value");
            Logger::debug("Full response: " + responseData);
        }
    } catch (const nlohmann::json::parse_error& e) {
        Logger::debug("JSON parse error: " + std::string(e.what()));
        Logger::debug("Raw response: " + responseData);
    }
    
    Logger::debug("Request failed with status code: " + std::to_string(httpCode));
    if (verbose) {
        fclose(verbose);
    }
    curl_slist_free_all(headers);
    return false;
}

// Helper function to encode string to base64
std::string VSphereRestClient::base64_encode(const std::string& input) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (unsigned char c : input) {
        char_array_3[i++] = c;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        
        while((i++ < 3))
            ret += '=';
    }
    
    return ret;
}

bool VSphereRestClient::logout() {
    if (!isLoggedIn_) {
        Logger::debug("Not logged in, skipping logout");
        return true;
    }
    
    Logger::debug("Attempting to logout from vCenter");
    nlohmann::json response;
    bool success = makeRequest("DELETE", "/rest/com/vmware/cis/session", nlohmann::json(), response);
    if (success) {
        isLoggedIn_ = false;
        sessionId_.clear();
        Logger::info("Successfully logged out from vCenter");
    } else {
        Logger::error("Failed to logout from vCenter");
    }
    return success;
}

std::string VSphereRestClient::getLastError() const {
    //TODO
    return "";
}

bool VSphereRestClient::makeRequestWithRetry(const std::string& method, const std::string& endpoint, 
                                           const nlohmann::json& data, nlohmann::json& response,
                                           int maxRetries) {
    Logger::debug("Making request with retry: " + method + " " + endpoint);
    Logger::debug("Max retries: " + std::to_string(maxRetries));
    
    for (int i = 0; i < maxRetries; i++) {
        Logger::debug("Attempt " + std::to_string(i + 1) + " of " + std::to_string(maxRetries));
        if (makeRequest(method, endpoint, data, response)) {
            Logger::debug("Request succeeded on attempt " + std::to_string(i + 1));
            return true;
        }
        if (i < maxRetries - 1) {
            Logger::warning("Request failed, attempt " + std::to_string(i + 1) + " of " + std::to_string(maxRetries));
            Logger::debug("Waiting 2 seconds before next retry");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    Logger::error("All retry attempts failed for request: " + method + " " + endpoint);
    return false;
}

bool VSphereRestClient::makeRequest(const std::string& method, const std::string& endpoint,
                                  const nlohmann::json& requestBody, nlohmann::json& response) {
    if (!curl_) {
        Logger::error("CURL not initialized");
        return false;
    }

    std::string url = "https://" + host_ + endpoint;
    Logger::debug("Making " + method + " request to: " + url);
    if (!requestBody.empty()) {
        Logger::debug("Request body: " + requestBody.dump());
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    // Add session ID if available
    if (!sessionId_.empty()) {
        std::string sessionHeader = "vmware-api-session-id: " + sessionId_;
        headers = curl_slist_append(headers, sessionHeader.c_str());
        Logger::debug("Added session ID to request: " + sessionId_);
    } else {
        Logger::warning("No session ID available for request");
    }

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(curl_, CURLOPT_TCP_KEEPINTVL, 30L);

    // Add verbose logging for CURL
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);
    FILE* verbose = fopen("/tmp/curl_verbose.log", "a");
    if (verbose) {
        fprintf(verbose, "\n=== Main Request ===\n");
        fprintf(verbose, "URL: %s\n", url.c_str());
        fprintf(verbose, "Method: %s\n", method.c_str());
        fprintf(verbose, "Headers:\n");
        fprintf(verbose, "  Content-Type: application/json\n");
        fprintf(verbose, "  Accept: application/json\n");
        if (!sessionId_.empty()) {
            fprintf(verbose, "  vmware-api-session-id: %s\n", sessionId_.c_str());
        }
        if (!requestBody.empty()) {
            fprintf(verbose, "Body: %s\n", requestBody.dump().c_str());
        }
        curl_easy_setopt(curl_, CURLOPT_STDERR, verbose);
    }

    std::string responseData;
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseData);

    if (method == "POST" || method == "PUT") {
        std::string jsonData = requestBody.dump();
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, jsonData.c_str());
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method.c_str());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl_);
    long httpCode = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &httpCode);

    if (verbose) {
        fprintf(verbose, "\nResponse Code: %ld\n", httpCode);
        fprintf(verbose, "Response Body: %s\n", responseData.c_str());
        fclose(verbose);
    }

    if (res != CURLE_OK) {
        Logger::error("Request failed: " + std::string(curl_easy_strerror(res)));
        curl_slist_free_all(headers);
        return false;
    }

    Logger::debug("Response code: " + std::to_string(httpCode));
    Logger::debug("Response body: " + responseData);

    // Handle session expiration (401 Unauthorized)
    if (httpCode == 401 && isLoggedIn_) {
        Logger::debug("Session expired, attempting to refresh");
        if (refreshSession()) {
            // Retry the request with the new session
            curl_slist_free_all(headers);
            return makeRequest(method, endpoint, requestBody, response);
        } else {
            Logger::error("Failed to refresh session");
            curl_slist_free_all(headers);
            return false;
        }
    }

    if (httpCode >= 200 && httpCode < 300) {
        try {
            response = nlohmann::json::parse(responseData);
            curl_slist_free_all(headers);
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to parse response: " + std::string(e.what()));
            curl_slist_free_all(headers);
            return false;
        }
    } else {
        Logger::error("Request failed with status code: " + std::to_string(httpCode));
        Logger::debug("Full response: " + responseData);
        curl_slist_free_all(headers);
        return false;
    }

    // This should never be reached, but added to satisfy the compiler
    return false;
}

std::string VSphereRestClient::buildUrl(const std::string& endpoint) const {
    return "https://" + host_ + endpoint;
}

void VSphereRestClient::setCommonHeaders(struct curl_slist*& headers) {
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!sessionId_.empty()) {
        std::string authHeader = "vmware-api-session-id: " + sessionId_;
        headers = curl_slist_append(headers, authHeader.c_str());
    }
}

bool VSphereRestClient::checkResponse(const nlohmann::json& response) const {
    return !response.contains("error");
}

void VSphereRestClient::handleError(const std::string& operation, const nlohmann::json& response) {
    std::string errorMsg = "Operation '" + operation + "' failed";
    if (response.contains("error")) {
        errorMsg += ": " + response["error"]["message"].get<std::string>();
    }
    Logger::error(errorMsg);
}

bool VSphereRestClient::getVMInfo(const std::string& vmId, nlohmann::json& vmInfo) {
    // Use filter.names in the URL query string
    std::string endpoint = "/rest/vcenter/vm?filter.names=" + vmId;
    nlohmann::json response;

    // Explicitly set GET method and ensure it's not overridden
    curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "GET");

    bool success = makeRequest("GET", endpoint, nlohmann::json(), response);

    // The response should contain the VM info directly
    if (response.contains("value") && !response["value"].empty()) {
        vmInfo = response["value"][0];  // Get the first (and should be only) VM
        return true;
    }

    Logger::error("VM not found: " + vmId);
    return false;
}

bool VSphereRestClient::getVMDiskPaths(const std::string& vmName, std::vector<std::string>& diskPaths) {
    Logger::info("Getting disk paths for VM: " + vmName);
    
    // Step 1: Get VM ID from VM name
    nlohmann::json vmInfo;
    if (!getVMInfo(vmName, vmInfo)) {
        Logger::error("Failed to get VM ID for VM: " + vmName);
        return false;
    }
    std::string vmId = vmInfo["vm"].get<std::string>();
    Logger::debug("Got VM ID: " + vmId + " for VM: " + vmName);

    // Step 2: Get disk numbers for the VM
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk", nlohmann::json(), response)) {
        Logger::error("Failed to get disk numbers for VM: " + vmId);
        return false;
    }

    // Parse disk numbers from response
    try {
        std::vector<std::string> diskNumbers;
        for (const auto& disk : response["value"]) {
            diskNumbers.push_back(disk["disk"].get<std::string>());
        }
        Logger::debug("Found " + std::to_string(diskNumbers.size()) + " disk(s)");

        // Step 3: Get disk path for each disk number
        for (const auto& diskNumber : diskNumbers) {
            nlohmann::json diskResponse;
            if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskNumber, nlohmann::json(), diskResponse)) {
                Logger::error("Failed to get disk path for disk " + diskNumber);
                continue;
            }

            // Parse disk path from response
            if (diskResponse["value"].contains("backing") && 
                diskResponse["value"]["backing"].contains("vmdk_file")) {
                std::string vmdkPath = diskResponse["value"]["backing"]["vmdk_file"].get<std::string>();
                
                // VDDK expects paths in the format: [datastore] path/to/vmdk
                // Example: [ogilvie_local_datastore01] jackrh8vm/jackrh8vm.vmdk
                // The path we get from the API is already in this format, so we can use it directly
                Logger::debug("Found disk path: " + vmdkPath);
                diskPaths.push_back(vmdkPath);
            }
        }

        if (diskPaths.empty()) {
            Logger::error("No valid disk paths found for VM: " + vmId);
            return false;
        }

        Logger::info("Successfully retrieved " + std::to_string(diskPaths.size()) + " disk path(s)");
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

bool VSphereRestClient::getVMDiskInfo(const std::string& vmId, const std::string& diskPath, nlohmann::json& diskInfo) {
    return makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskPath, nlohmann::json(), diskInfo);
}

bool VSphereRestClient::enableCBT(const std::string& vmId) {
    Logger::info("Enabling CBT for VM: " + vmId);
    try {
        std::string endpoint = "/rest/vcenter/vm/" + vmId + "/config";
        nlohmann::json requestBody = {
            {"changed_block_tracking_enabled", true}
        };
        
        nlohmann::json response;
        bool success = makeRequest("PATCH", endpoint, requestBody, response);
        if (success) {
            Logger::info("Successfully enabled CBT for VM: " + vmId);
        } else {
            Logger::error("Failed to enable CBT for VM: " + vmId);
        }
        return success;
    } catch (const std::exception& e) {
        Logger::error("Exception while enabling CBT: " + std::string(e.what()));
        return false;
    }
}

bool VSphereRestClient::disableCBT(const std::string& vmId) {
    Logger::info("Disabling CBT for VM: " + vmId);
    try {
        std::string endpoint = "/rest/vcenter/vm/" + vmId + "/config";
        nlohmann::json requestBody = {
            {"changed_block_tracking_enabled", false}
        };
        
        nlohmann::json response;
        bool success = makeRequest("PATCH", endpoint, requestBody, response);
        if (success) {
            Logger::info("Successfully disabled CBT for VM: " + vmId);
        } else {
            Logger::error("Failed to disable CBT for VM: " + vmId);
        }
        return success;
    } catch (const std::exception& e) {
        Logger::error("Exception while disabling CBT: " + std::string(e.what()));
        return false;
    }
}

bool VSphereRestClient::isCBTEnabled(const std::string& vmId) {
    Logger::info("Checking CBT status for VM: " + vmId);
    try {
        std::string endpoint = "/rest/vcenter/vm/" + vmId + "/config";
        nlohmann::json response;
        bool success = makeRequest("GET", endpoint, nlohmann::json(), response);
        if (success && response.contains("changed_block_tracking_enabled")) {
            bool enabled = response["changed_block_tracking_enabled"];
            Logger::info("CBT status for VM " + vmId + ": " + (enabled ? "enabled" : "disabled"));
            return enabled;
        }
        Logger::error("Failed to get CBT status for VM: " + vmId);
        return false;
    } catch (const std::exception& e) {
        Logger::error("Exception while checking CBT status: " + std::string(e.what()));
        return false;
    }
}

bool VSphereRestClient::getVMPowerState(const std::string& vmId, std::string& powerState) {
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/power", nlohmann::json(), response)) {
        return false;
    }
    powerState = response["value"].get<std::string>();
    return true;
}

bool VSphereRestClient::powerOnVM(const std::string& vmId) {
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/start", nlohmann::json(), response);
}

bool VSphereRestClient::powerOffVM(const std::string& vmId) {
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/stop", nlohmann::json(), response);
}

bool VSphereRestClient::suspendVM(const std::string& vmId) {
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/suspend", nlohmann::json(), response);
}

bool VSphereRestClient::resetVM(const std::string& vmId) {
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/reset", nlohmann::json(), response);
}

bool VSphereRestClient::shutdownVM(const std::string& vmId) {
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/shutdown", nlohmann::json(), response);
}

bool VSphereRestClient::rebootVM(const std::string& vmId) {
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/reboot", nlohmann::json(), response);
}

bool VSphereRestClient::createSnapshot(const std::string& vmId, const std::string& name, const std::string& description) {
    // FixMe: For now, we don't create snapshots, we just return true
    //nlohmann::json data = {
    //    {"name", name},
    //    {"description", description}
    //};
    //nlohmann::json response;
    //return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/snapshot", data, response);

    return true;
}

bool VSphereRestClient::removeSnapshot(const std::string& vmId, const std::string& snapshotId) {
    // FixMe: For now, we don't remove snapshots, we just return true
    //nlohmann::json response;
    //return makeRequest("DELETE", "/rest/vcenter/vm/" + vmId + "/snapshot/" + snapshotId, nlohmann::json(), response);
    return true;
}

bool VSphereRestClient::revertToSnapshot(const std::string& vmId, const std::string& snapshotId) {
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/snapshot/" + snapshotId + "/revert", nlohmann::json(), response);
}

bool VSphereRestClient::getSnapshots(const std::string& vmId, nlohmann::json& snapshots) {
    return makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/snapshot", nlohmann::json(), snapshots);
}

bool VSphereRestClient::getVMNetworks(const std::string& vmId, std::vector<std::string>& networks) {
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/ethernet", nlohmann::json(), response)) {
        return false;
    }
    
    for (const auto& network : response["value"]) {
        networks.push_back(network["value"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getDatastores(std::vector<std::string>& datastores) {
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/datastore", nlohmann::json(), response)) {
        return false;
    }
    
    for (const auto& datastore : response["value"]) {
        datastores.push_back(datastore["datastore"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getNetworks(std::vector<std::string>& networks) {
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/network", nlohmann::json(), response)) {
        return false;
    }
    
    for (const auto& network : response["value"]) {
        networks.push_back(network["network"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getResourcePools(std::vector<std::string>& resourcePools) {
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/resource-pool", nlohmann::json(), response)) {
        return false;
    }
    
    for (const auto& pool : response["value"]) {
        resourcePools.push_back(pool["resource_pool"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getHosts(std::vector<std::string>& hosts) {
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/host", nlohmann::json(), response)) {
        return false;
    }
    
    for (const auto& host : response["value"]) {
        hosts.push_back(host["host"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::prepareVMForBackup(const std::string& vmId, bool quiesce) {
    // Create a snapshot for backup
    nlohmann::json data = {
        {"name", "backup-snapshot"},
        {"description", "Snapshot created for backup"},
        {"quiesce", quiesce}
    };
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/snapshot", data, response);
}

bool VSphereRestClient::cleanupVMAfterBackup(const std::string& vmId) {
    // Get the backup snapshot
    nlohmann::json snapshots;
    if (!getSnapshots(vmId, snapshots)) {
        return false;
    }
    
    // Find and remove the backup snapshot
    for (const auto& snapshot : snapshots["value"]) {
        if (snapshot["name"] == "backup-snapshot") {
            return removeSnapshot(vmId, snapshot["snapshot"].get<std::string>());
        }
    }
    return false;
}

bool VSphereRestClient::createVM(const nlohmann::json& vmConfig, nlohmann::json& response) {
    // Validate required fields
    if (!vmConfig.contains("name") || !vmConfig.contains("datastore_id") || 
        !vmConfig.contains("resource_pool_id")) {
        Logger::error("Missing required fields in VM configuration");
        return false;
    }

    // Validate field types
    if (!vmConfig["name"].is_string() || !vmConfig["datastore_id"].is_string() || 
        !vmConfig["resource_pool_id"].is_string()) {
        Logger::error("Invalid field types in VM configuration");
        return false;
    }

    // Validate optional fields if present
    if (vmConfig.contains("num_cpus") && !vmConfig["num_cpus"].is_number()) {
        Logger::error("Invalid num_cpus field type");
        return false;
    }
    if (vmConfig.contains("memory_mb") && !vmConfig["memory_mb"].is_number()) {
        Logger::error("Invalid memory_mb field type");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/vm", vmConfig, response);
    if (success) {
        Logger::info("Successfully created VM: " + vmConfig["name"].get<std::string>());
    }
    return success;
}

bool VSphereRestClient::attachDisk(const std::string& vmId, const nlohmann::json& diskConfig, nlohmann::json& response) {
    // Validate VM ID
    if (vmId.empty()) {
        Logger::error("Invalid VM ID");
        return false;
    }

    // Validate required fields
    if (!diskConfig.contains("path")) {
        Logger::error("Missing required disk path in configuration");
        return false;
    }

    // Validate field types
    if (!diskConfig["path"].is_string()) {
        Logger::error("Invalid disk path field type");
        return false;
    }

    // Validate optional fields if present
    if (diskConfig.contains("controller_type") && !diskConfig["controller_type"].is_string()) {
        Logger::error("Invalid controller_type field type");
        return false;
    }
    if (diskConfig.contains("unit_number") && !diskConfig["unit_number"].is_number()) {
        Logger::error("Invalid unit_number field type");
        return false;
    }
    if (diskConfig.contains("thin_provisioned") && !diskConfig["thin_provisioned"].is_boolean()) {
        Logger::error("Invalid thin_provisioned field type");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/hardware/disk", diskConfig, response);
    if (success) {
        Logger::info("Successfully attached disk to VM: " + vmId);
    }
    return success;
}

bool VSphereRestClient::listVMs(nlohmann::json& response) {
    // Add query parameters for filtering and pagination
    nlohmann::json queryParams = {
        {"filter.names", nlohmann::json::array()},  // Empty array means no name filtering
        {"filter.power_states", nlohmann::json::array()},  // Empty array means all power states
        {"page_size", 100},  // Maximum number of VMs to return
        {"page", 1}  // Start with first page
    };

    bool success = makeRequest("GET", "/rest/vcenter/vm", queryParams, response);
    if (success) {
        Logger::info("Successfully retrieved VM list");
    }
    return success;
}

// Add new method for VM cloning
bool VSphereRestClient::cloneVM(const std::string& sourceVmId, const nlohmann::json& cloneConfig, nlohmann::json& response) {
    // Validate source VM ID
    if (sourceVmId.empty()) {
        Logger::error("Invalid source VM ID");
        return false;
    }

    // Validate required fields
    if (!cloneConfig.contains("name") || !cloneConfig.contains("datastore_id") || 
        !cloneConfig.contains("resource_pool_id")) {
        Logger::error("Missing required fields in clone configuration");
        return false;
    }

    // Validate field types
    if (!cloneConfig["name"].is_string() || !cloneConfig["datastore_id"].is_string() || 
        !cloneConfig["resource_pool_id"].is_string()) {
        Logger::error("Invalid field types in clone configuration");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/vm/" + sourceVmId + "/clone", cloneConfig, response);
    if (success) {
        Logger::info("Successfully cloned VM: " + sourceVmId + " to: " + cloneConfig["name"].get<std::string>());
    }
    return success;
}

// Add new method for VM migration
bool VSphereRestClient::migrateVM(const std::string& vmId, const nlohmann::json& migrateConfig, nlohmann::json& response) {
    // Validate VM ID
    if (vmId.empty()) {
        Logger::error("Invalid VM ID");
        return false;
    }

    // Validate required fields
    if (!migrateConfig.contains("target_host") || !migrateConfig.contains("target_datastore")) {
        Logger::error("Missing required fields in migration configuration");
        return false;
    }

    // Validate field types
    if (!migrateConfig["target_host"].is_string() || !migrateConfig["target_datastore"].is_string()) {
        Logger::error("Invalid field types in migration configuration");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/migrate", migrateConfig, response);
    if (success) {
        Logger::info("Successfully initiated VM migration: " + vmId);
    }
    return success;
}

bool VSphereRestClient::getChangedDiskAreas(const std::string& vmId, const std::string& diskId,
                                          int64_t startOffset, int64_t length, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty() || diskId.empty() || startOffset < 0 || length <= 0) {
        Logger::error("Invalid input parameters for getting changed disk areas");
        return false;
    }

    nlohmann::json params = {
        {"start_offset", startOffset},
        {"length", length}
    };

    bool success = makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskId + "/changed-areas",
                             params, response);
    if (success) {
        Logger::info("Successfully retrieved changed areas for disk " + diskId);
    }
    return success;
}

bool VSphereRestClient::getDiskLayout(const std::string& vmId, const std::string& diskId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty() || diskId.empty()) {
        Logger::error("Invalid input parameters for getting disk layout");
        return false;
    }

    bool success = makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskId + "/layout",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully retrieved layout for disk " + diskId);
    }
    return success;
}

bool VSphereRestClient::getDiskChainInfo(const std::string& vmId, const std::string& diskId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty() || diskId.empty()) {
        Logger::error("Invalid input parameters for getting disk chain info");
        return false;
    }

    bool success = makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskId + "/chain",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully retrieved chain info for disk " + diskId);
    }
    return success;
}

bool VSphereRestClient::consolidateDisks(const std::string& vmId, const std::string& diskId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty() || diskId.empty()) {
        Logger::error("Invalid input parameters for disk consolidation");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskId + "/consolidate",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully initiated consolidation for disk " + diskId);
    }
    return success;
}

bool VSphereRestClient::defragmentDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty() || diskId.empty()) {
        Logger::error("Invalid input parameters for disk defragmentation");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskId + "/defragment",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully initiated defragmentation for disk " + diskId);
    }
    return success;
}

bool VSphereRestClient::shrinkDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty() || diskId.empty()) {
        Logger::error("Invalid input parameters for disk shrinking");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskId + "/shrink",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully initiated shrinking for disk " + diskId);
    }
    return success;
}

bool VSphereRestClient::getBackupProgress(const std::string& taskId, nlohmann::json& response) {
    // Validate inputs
    if (taskId.empty()) {
        Logger::error("Invalid task ID for getting backup progress");
        return false;
    }

    bool success = makeRequest("GET", "/rest/vcenter/backup/task/" + taskId + "/progress",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully retrieved progress for backup task " + taskId);
    }
    return success;
}

bool VSphereRestClient::cancelBackup(const std::string& taskId, nlohmann::json& response) {
    // Validate inputs
    if (taskId.empty()) {
        Logger::error("Invalid task ID for canceling backup");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/backup/task/" + taskId + "/cancel",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully canceled backup task " + taskId);
    }
    return success;
}

bool VSphereRestClient::verifyBackup(const std::string& backupId, nlohmann::json& response) {
    // Validate inputs
    if (backupId.empty()) {
        Logger::error("Invalid backup ID for verification");
        return false;
    }

    bool success = makeRequest("POST", "/rest/vcenter/backup/" + backupId + "/verify",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully initiated verification for backup " + backupId);
    }
    return success;
}

bool VSphereRestClient::getBackupHistory(const std::string& vmId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty()) {
        Logger::error("Invalid VM ID for getting backup history");
        return false;
    }

    bool success = makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/backup/history",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully retrieved backup history for VM " + vmId);
    }
    return success;
}

bool VSphereRestClient::getBackupSchedule(const std::string& vmId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty()) {
        Logger::error("Invalid VM ID for getting backup schedule");
        return false;
    }

    bool success = makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/backup/schedule",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully retrieved backup schedule for VM " + vmId);
    }
    return success;
}

bool VSphereRestClient::setBackupSchedule(const std::string& vmId, const nlohmann::json& schedule, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty()) {
        Logger::error("Invalid VM ID for setting backup schedule");
        return false;
    }

    // Validate schedule configuration
    if (!schedule.contains("frequency") || !schedule.contains("time")) {
        Logger::error("Missing required fields in schedule configuration");
        return false;
    }

    bool success = makeRequest("PUT", "/rest/vcenter/vm/" + vmId + "/backup/schedule",
                             schedule, response);
    if (success) {
        Logger::info("Successfully set backup schedule for VM " + vmId);
    }
    return success;
}

bool VSphereRestClient::getBackupRetention(const std::string& vmId, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty()) {
        Logger::error("Invalid VM ID for getting backup retention");
        return false;
    }

    bool success = makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/backup/retention",
                             nlohmann::json(), response);
    if (success) {
        Logger::info("Successfully retrieved backup retention for VM " + vmId);
    }
    return success;
}

bool VSphereRestClient::setBackupRetention(const std::string& vmId, const nlohmann::json& retention, nlohmann::json& response) {
    // Validate inputs
    if (vmId.empty()) {
        Logger::error("Invalid VM ID for setting backup retention");
        return false;
    }

    // Validate retention configuration
    if (!retention.contains("days") || !retention.contains("copies")) {
        Logger::error("Missing required fields in retention configuration");
        return false;
    }

    bool success = makeRequest("PUT", "/rest/vcenter/vm/" + vmId + "/backup/retention",
                             retention, response);
    if (success) {
        Logger::info("Successfully set backup retention for VM " + vmId);
    }
    return success;
}

bool VSphereRestClient::createDisk(const std::string& vmId, const nlohmann::json& diskConfig, nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk";
    return makeRequest("POST", endpoint, diskConfig, response);
}

bool VSphereRestClient::resizeDisk(const std::string& vmId, const std::string& diskId, int64_t newSizeKB, nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk/" + diskId;
    nlohmann::json data = {
        {"size_kb", newSizeKB}
    };
    return makeRequest("PATCH", endpoint, data, response);
}

bool VSphereRestClient::deleteDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk/" + diskId;
    return makeRequest("DELETE", endpoint, nlohmann::json(), response);
}

bool VSphereRestClient::detachDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk/" + diskId + "/detach";
    return makeRequest("POST", endpoint, nlohmann::json(), response);
}

bool VSphereRestClient::updateDiskBacking(const std::string& vmId, const std::string& diskId, 
                                        const nlohmann::json& backingConfig, nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk/" + diskId + "/backing";
    return makeRequest("PATCH", endpoint, backingConfig, response);
}

bool VSphereRestClient::getDiskControllers(const std::string& vmId, nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk/controllers";
    return makeRequest("GET", endpoint, nlohmann::json(), response);
}

bool VSphereRestClient::createDiskController(const std::string& vmId, const nlohmann::json& controllerConfig, 
                                           nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk/controllers";
    return makeRequest("POST", endpoint, controllerConfig, response);
}

bool VSphereRestClient::deleteDiskController(const std::string& vmId, const std::string& controllerId, 
                                           nlohmann::json& response) {
    std::string endpoint = "/vcenter/vm/" + vmId + "/disk/controllers/" + controllerId;
    return makeRequest("DELETE", endpoint, nlohmann::json(), response);
}

size_t VSphereRestClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}

bool VSphereRestClient::getBackup(const std::string& backupId, std::string& response) {
    if (!isLoggedIn_) {
        lastError_ = "Not logged in to vSphere";
        return false;
    }

    nlohmann::json jsonResponse;
    bool success = makeRequest("GET", "/rest/vcenter/backup/" + backupId, nlohmann::json(), jsonResponse);
    if (success) {
        response = jsonResponse.dump();
    }
    return success;
}

bool VSphereRestClient::refreshSession() {
    if (!isLoggedIn_) {
        Logger::debug("Cannot refresh session: not logged in");
        return false;
    }
    
    Logger::debug("Attempting to refresh session");
    nlohmann::json response;
    if (makeRequest("POST", "/rest/com/vmware/cis/session/refresh", nlohmann::json(), response)) {
        try {
            std::string oldSessionId = sessionId_;
            sessionId_ = response["value"].get<std::string>();
            Logger::debug("Successfully refreshed session");
            Logger::debug("Old session ID: " + oldSessionId);
            Logger::debug("New session ID: " + sessionId_);
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to parse session refresh response: " + std::string(e.what()));
            Logger::debug("Raw response: " + response.dump());
            return false;
        }
    }
    Logger::error("Failed to refresh session");
    return false;
}

void VSphereRestClient::logTokenInfo(const std::string& operation, const std::string& tokenType) {
    Logger::debug(tokenType + " token " + operation + ":");
    if (tokenType == "STS") {
        auto timeLeft = std::chrono::duration_cast<std::chrono::minutes>(
            stsTokenExpiry_ - std::chrono::system_clock::now());
        Logger::debug("  Expires in: " + std::to_string(timeLeft.count()) + " minutes");
    }
}
