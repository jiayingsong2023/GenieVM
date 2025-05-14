#include "common/vsphere_rest_client.hpp"
#include "common/logger.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>

VSphereRestClient::VSphereRestClient(const std::string& host, const std::string& username, const std::string& password)
    : host_(host), username_(username), password_(password), curl_(nullptr), isLoggedIn_(false) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
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
    if (isLoggedIn_) {
        return true;
    }

    nlohmann::json body = {
        {"username", username_},
        {"password", password_}
    };

    nlohmann::json response;
    if (!makeRequest("POST", "/rest/com/vmware/cis/session", body, response)) {
        return false;
    }

    sessionId_ = response["value"].get<std::string>();
    isLoggedIn_ = true;
    return true;
}

void VSphereRestClient::logout() {
    if (!isLoggedIn_) {
        return;
    }

    nlohmann::json response;
    makeRequest("DELETE", "/rest/com/vmware/cis/session", response);
    sessionId_.clear();
    isLoggedIn_ = false;
}

bool VSphereRestClient::connect() {
    // TODO: Implement connection logic
    return true;
}

void VSphereRestClient::disconnect() {
    // TODO: Implement disconnect logic
}

std::string VSphereRestClient::makeRequest(const std::string& endpoint, const std::string& method, const std::string& data) {
    // TODO: Implement REST request logic using libcurl
    return "";
}

// Example VM operation
bool VSphereRestClient::powerOnVM(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/start", response);
}

// Add other methods as needed, following the same pattern

bool VSphereRestClient::powerOffVM(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/stop", response);
}

bool VSphereRestClient::suspendVM(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/suspend", response);
}

bool VSphereRestClient::resetVM(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/reset", response);
}

bool VSphereRestClient::shutdownVM(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/shutdown", response);
}

bool VSphereRestClient::rebootVM(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/power/reboot", response);
}

bool VSphereRestClient::getVMInfo(const std::string& vmId, nlohmann::json& info) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    return makeRequest("GET", "/rest/vcenter/vm/" + vmId, info);
}

bool VSphereRestClient::getVMPowerState(const std::string& vmId, std::string& state) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/power", response)) {
        return false;
    }

    state = response["value"]["state"].get<std::string>();
    return true;
}

bool VSphereRestClient::getVMDisks(const std::string& vmId, std::vector<std::string>& diskPaths) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk", response)) {
        return false;
    }

    for (const auto& disk : response["value"]) {
        diskPaths.push_back(disk["backing"]["vmdk_file"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getVMNetworks(const std::string& vmId, std::vector<std::string>& networks) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/ethernet", response)) {
        return false;
    }

    networks.clear();
    for (const auto& nic : response["value"]) {
        networks.push_back(nic["backing"]["network"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::createSnapshot(const std::string& vmId, const std::string& name, const std::string& description) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json body = {
        {"name", name},
        {"description", description},
        {"memory", false},
        {"quiesce", true}
    };
    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/snapshot", body, response);
}

bool VSphereRestClient::removeSnapshot(const std::string& vmId, const std::string& snapshotName) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    // First, get the snapshot ID
    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/snapshot", response)) {
        return false;
    }

    std::string snapshotId;
    for (const auto& snapshot : response["value"]) {
        if (snapshot["name"].get<std::string>() == snapshotName) {
            snapshotId = snapshot["snapshot"].get<std::string>();
            break;
        }
    }

    if (snapshotId.empty()) {
        Logger::error("Snapshot not found: " + snapshotName);
        return false;
    }

    return makeRequest("DELETE", "/rest/vcenter/vm/" + vmId + "/snapshot/" + snapshotId, response);
}

bool VSphereRestClient::revertToSnapshot(const std::string& vmId, const std::string& snapshotId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    return makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/action/revert", response);
}

bool VSphereRestClient::getSnapshots(const std::string& vmId, nlohmann::json& snapshots) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    return makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/snapshot", snapshots);
}

bool VSphereRestClient::getDatastores(std::vector<std::string>& datastores) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/datastore", response)) {
        return false;
    }

    datastores.clear();
    for (const auto& ds : response["value"]) {
        datastores.push_back(ds["datastore"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getNetworks(std::vector<std::string>& networks) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/network", response)) {
        return false;
    }

    networks.clear();
    for (const auto& net : response["value"]) {
        networks.push_back(net["network"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getResourcePools(std::vector<std::string>& resourcePools) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/resource-pool", response)) {
        return false;
    }

    resourcePools.clear();
    for (const auto& rp : response["value"]) {
        resourcePools.push_back(rp["resource_pool"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getHosts(std::vector<std::string>& hosts) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/host", response)) {
        return false;
    }

    hosts.clear();
    for (const auto& host : response["value"]) {
        hosts.push_back(host["host"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::prepareVMForBackup(const std::string& vmId, bool quiesce) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    // Create a snapshot for backup
    std::stringstream ss;
    auto now = std::time(nullptr);
    ss << "Backup_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
    
    nlohmann::json body = {
        {"name", ss.str()},
        {"description", "Backup snapshot"},
        {"memory", false},
        {"quiesce", quiesce}
    };

    nlohmann::json response;
    if (!makeRequest("POST", "/rest/vcenter/vm/" + vmId + "/snapshot", body, response)) {
        return false;
    }

    // Store snapshot ID for cleanup
    snapshotId_ = response["value"].get<std::string>();
    return true;
}

bool VSphereRestClient::cleanupVMAfterBackup(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    if (snapshotId_.empty()) {
        Logger::error("No snapshot ID found for cleanup");
        return false;
    }

    nlohmann::json response;
    bool success = makeRequest("DELETE", "/rest/vcenter/vm/" + vmId + "/snapshot/" + snapshotId_, response);
    snapshotId_.clear();
    return success;
}

bool VSphereRestClient::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk", response)) {
        return false;
    }

    for (const auto& disk : response["value"]) {
        diskPaths.push_back(disk["backing"]["vmdk_file"].get<std::string>());
    }
    return true;
}

bool VSphereRestClient::getVMDiskInfo(const std::string& vmId, const std::string& diskPath, nlohmann::json& info) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json response;
    if (!makeRequest("GET", "/rest/vcenter/vm/" + vmId + "/hardware/disk", response)) {
        return false;
    }

    for (const auto& disk : response["value"]) {
        if (disk["backing"]["vmdk_file"].get<std::string>() == diskPath) {
            info = disk;
            return true;
        }
    }
    return false;
}

bool VSphereRestClient::enableCBT(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json body = {
        {"enabled", true}
    };

    nlohmann::json response;
    return makeRequest("PATCH", "/rest/vcenter/vm/" + vmId + "/hardware/disk/0", body, response);
}

bool VSphereRestClient::disableCBT(const std::string& vmId) {
    if (!isLoggedIn_ && !login()) {
        return false;
    }

    nlohmann::json body = {
        {"enabled", false}
    };

    nlohmann::json response;
    return makeRequest("PATCH", "/rest/vcenter/vm/" + vmId + "/hardware/disk/0", body, response);
}

bool VSphereRestClient::makeRequest(const std::string& method, const std::string& endpoint, 
                                  const nlohmann::json& body, nlohmann::json& response) {
    std::string url = buildUrl(endpoint);
    std::string bodyStr = body.dump();
    std::string responseStr;

    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &responseStr);

    if (!bodyStr.empty()) {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, bodyStr.c_str());
    }

    setCommonHeaders(curl_);

    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        Logger::error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        Logger::error("HTTP request failed with code " + std::to_string(http_code));
        return false;
    }

    if (!responseStr.empty()) {
        try {
            response = nlohmann::json::parse(responseStr);
        } catch (const std::exception& e) {
            Logger::error("Failed to parse JSON response: " + std::string(e.what()));
            return false;
        }
    }

    return true;
}

bool VSphereRestClient::makeRequest(const std::string& method, const std::string& endpoint, 
                                  nlohmann::json& response) {
    return makeRequest(method, endpoint, nlohmann::json(), response);
}

std::string VSphereRestClient::buildUrl(const std::string& endpoint) {
    return "https://" + host_ + endpoint;
}

void VSphereRestClient::setCommonHeaders(CURL* curl) {
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    
    if (!sessionId_.empty()) {
        std::string authHeader = "vmware-api-session-id: " + sessionId_;
        headers = curl_slist_append(headers, authHeader.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

size_t VSphereRestClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
} 