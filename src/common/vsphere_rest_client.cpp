#include "common/vsphere_rest_client.hpp"
#include "common/logger.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>

VSphereRestClient::VSphereRestClient(const std::string& host, const std::string& username, const std::string& password)
    : host_(host), username_(username), password_(password), curl_(nullptr), isLoggedIn_(false) {
    curl_global_init(CURL_GLOBAL_ALL);
    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL");
    }
}

VSphereRestClient::~VSphereRestClient() {
    if (isLoggedIn_) {
        logout();
    }
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
    curl_global_cleanup();
}

bool VSphereRestClient::login() {
    nlohmann::json loginData = {
        {"username", username_},
        {"password", password_}
    };
    
    nlohmann::json response;
    if (!makeRequest("POST", "/rest/com/vmware/cis/session", loginData, response)) {
        return false;
    }
    
    sessionId_ = response["value"].get<std::string>();
    isLoggedIn_ = true;
    return true;
}

bool VSphereRestClient::logout() {
    if (!isLoggedIn_) {
        return true;
    }
    
    nlohmann::json response;
    bool success = makeRequest("DELETE", "/rest/com/vmware/cis/session", nlohmann::json(), response);
    if (success) {
        isLoggedIn_ = false;
        sessionId_.clear();
    }
    return success;
}

bool VSphereRestClient::makeRequest(const std::string& method, const std::string& endpoint, 
                                  const nlohmann::json& data, nlohmann::json& response) {
    if (!curl_) {
        return false;
    }

    std::string url = buildUrl(endpoint);
    std::string responseData;
    
    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseData);
    
    struct curl_slist* headers = nullptr;
    setCommonHeaders(headers);
    
    if (method == "POST" || method == "PUT") {
        std::string jsonData = data.dump();
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, jsonData.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        handleError(method + " " + endpoint, nlohmann::json::object());
        return false;
    }
    
    try {
        response = nlohmann::json::parse(responseData);
        if (!checkResponse(response)) {
            handleError(method + " " + endpoint, response);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        handleError(method + " " + endpoint, nlohmann::json::object());
        return false;
    }
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
    return makeRequest("GET", "/rest/vcenter/vm/" + vmId, nlohmann::json(), vmInfo);
}

bool VSphereRestClient::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) {
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk", nlohmann::json(), response)) {
        return false;
    }
    
    for (const auto& disk : response["value"]) {
        diskPaths.push_back(disk["value"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getVMDiskInfo(const std::string& vmId, const std::string& diskPath, nlohmann::json& diskInfo) {
    return makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk/" + diskPath, nlohmann::json(), diskInfo);
}

bool VSphereRestClient::enableCBT(const std::string& vmId) {
    nlohmann::json data = {
        {"enabled", true}
    };
    nlohmann::json response;
    return makeRequest("PATCH", "/rest/vcenter/vm/" + vmId + "/hardware/disk/change-tracking", data, response);
}

bool VSphereRestClient::disableCBT(const std::string& vmId) {
    nlohmann::json data = {
        {"enabled", false}
    };
    nlohmann::json response;
    return makeRequest("PATCH", "/rest/vcenter/vm/" + vmId + "/hardware/disk/change-tracking", data, response);
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
    nlohmann::json data = {
        {"name", name},
        {"description", description}
    };
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/snapshot", data, response);
}

bool VSphereRestClient::removeSnapshot(const std::string& vmId, const std::string& snapshotId) {
    nlohmann::json response;
    return makeRequest("DELETE", "/rest/vcenter/vm/" + vmId + "/snapshot/" + snapshotId, nlohmann::json(), response);
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

size_t VSphereRestClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}