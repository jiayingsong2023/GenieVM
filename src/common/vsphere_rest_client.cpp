#include "common/vsphere_rest_client.hpp"
#include "common/logger.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>

VSphereRestClient::VSphereRestClient(const std::string& host, const std::string& username, const std::string& password)
    : host_(host), username_(username), password_(password) {}

VSphereRestClient::~VSphereRestClient() {}

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
    // TODO: Implement power on logic
    Logger::info("Powering on VM: " + vmId);
    return true;
}

// Add other methods as needed, following the same pattern

bool VSphereRestClient::powerOffVM(const std::string& vmId) {
    nlohmann::json body = {
        {"action", "power-off"}
    };
    nlohmann::json response;
    return makeRequest("POST", "/vcenter/vm/" + vmId + "/power", body, response);
}

bool VSphereRestClient::suspendVM(const std::string& vmId) {
    nlohmann::json body = {
        {"action", "suspend"}
    };
    nlohmann::json response;
    return makeRequest("POST", "/vcenter/vm/" + vmId + "/power", body, response);
}

bool VSphereRestClient::resetVM(const std::string& vmId) {
    nlohmann::json body = {
        {"action", "reset"}
    };
    nlohmann::json response;
    return makeRequest("POST", "/vcenter/vm/" + vmId + "/power", body, response);
}

bool VSphereRestClient::shutdownVM(const std::string& vmId) {
    nlohmann::json body = {
        {"action", "shutdown"}
    };
    nlohmann::json response;
    return makeRequest("POST", "/vcenter/vm/" + vmId + "/power", body, response);
}

bool VSphereRestClient::rebootVM(const std::string& vmId) {
    nlohmann::json body = {
        {"action", "reboot"}
    };
    nlohmann::json response;
    return makeRequest("POST", "/vcenter/vm/" + vmId + "/power", body, response);
}

bool VSphereRestClient::getVMInfo(const std::string& vmId, nlohmann::json& info) {
    return makeRequest("GET", "/vcenter/vm/" + vmId, info);
}

bool VSphereRestClient::getVMPowerState(const std::string& vmId, std::string& state) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/vm/" + vmId + "/power", response)) {
        return false;
    }
    state = response["state"];
    return true;
}

bool VSphereRestClient::getVMDisks(const std::string& vmId, std::vector<std::string>& diskPaths) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/vm/" + vmId + "/hardware/disk", response)) {
        return false;
    }

    diskPaths.clear();
    for (const auto& disk : response) {
        diskPaths.push_back(disk["backing"]["vmdk_file"]);
    }
    return true;
}

bool VSphereRestClient::getVMNetworks(const std::string& vmId, std::vector<std::string>& networks) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/vm/" + vmId + "/hardware/ethernet", response)) {
        return false;
    }

    networks.clear();
    for (const auto& nic : response) {
        networks.push_back(nic["backing"]["network"]);
    }
    return true;
}

bool VSphereRestClient::createSnapshot(const std::string& vmId, const std::string& name, const std::string& description) {
    nlohmann::json body = {
        {"name", name},
        {"description", description},
        {"memory", false},
        {"quiesce", true}
    };
    nlohmann::json response;
    return makeRequest("POST", "/vcenter/vm/" + vmId + "/snapshot", body, response);
}

bool VSphereRestClient::removeSnapshot(const std::string& vmId, const std::string& snapshotId) {
    nlohmann::json response;
    return makeRequest("DELETE", "/vcenter/vm/" + vmId + "/snapshot/" + snapshotId, response);
}

bool VSphereRestClient::revertToSnapshot(const std::string& vmId, const std::string& snapshotId) {
    nlohmann::json body = {
        {"snapshot", snapshotId}
    };
    nlohmann::json response;
    return makeRequest("POST", "/vcenter/vm/" + vmId + "/action/revert", body, response);
}

bool VSphereRestClient::getSnapshots(const std::string& vmId, nlohmann::json& snapshots) {
    return makeRequest("GET", "/vcenter/vm/" + vmId + "/snapshot", snapshots);
}

bool VSphereRestClient::getDatastores(std::vector<std::string>& datastores) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/datastore", response)) {
        return false;
    }

    datastores.clear();
    for (const auto& ds : response) {
        datastores.push_back(ds["datastore"]);
    }
    return true;
}

bool VSphereRestClient::getNetworks(std::vector<std::string>& networks) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/network", response)) {
        return false;
    }

    networks.clear();
    for (const auto& net : response) {
        networks.push_back(net["network"]);
    }
    return true;
}

bool VSphereRestClient::getResourcePools(std::vector<std::string>& resourcePools) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/resource-pool", response)) {
        return false;
    }

    resourcePools.clear();
    for (const auto& rp : response) {
        resourcePools.push_back(rp["resource_pool"]);
    }
    return true;
}

bool VSphereRestClient::getHosts(std::vector<std::string>& hosts) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/host", response)) {
        return false;
    }

    hosts.clear();
    for (const auto& host : response) {
        hosts.push_back(host["host"]);
    }
    return true;
}

bool VSphereRestClient::prepareVMForBackup(const std::string& vmId, bool quiesce) {
    // Create a snapshot for backup
    std::stringstream ss;
    ss << "Backup_" << std::put_time(std::localtime(&std::time(nullptr)), "%Y%m%d_%H%M%S");
    
    nlohmann::json response;
    if (!createSnapshot(vmId, ss.str(), "Backup snapshot", response)) {
        return false;
    }

    // Store snapshot ID for cleanup
    // TODO: Store snapshot ID in a way that can be retrieved during cleanup
    return true;
}

bool VSphereRestClient::cleanupVMAfterBackup(const std::string& vmId) {
    // TODO: Get the snapshot ID created during prepareVMForBackup
    std::string snapshotId = "TODO";
    
    return removeSnapshot(vmId, snapshotId);
}

bool VSphereRestClient::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) {
    return getVMDisks(vmId, diskPaths);
}

bool VSphereRestClient::getVMDiskInfo(const std::string& vmId, const std::string& diskPath, nlohmann::json& info) {
    nlohmann::json response;
    if (!makeRequest("GET", "/vcenter/vm/" + vmId + "/hardware/disk", response)) {
        return false;
    }

    for (const auto& disk : response) {
        if (disk["backing"]["vmdk_file"] == diskPath) {
            info = disk;
            return true;
        }
    }
    return false;
}

bool VSphereRestClient::makeRequest(const std::string& method, const std::string& path, 
                                  const nlohmann::json& body, nlohmann::json& response) {
    if (!connect()) {
        return false;
    }

    std::string url = getBaseUrl() + path;
    std::string responseStr;
    
    curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &responseStr);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, getAuthHeader().c_str());
    curl_easy_setopt(curl_.get(), CURLOPT_HTTPHEADER, headers);

    if (!body.is_null()) {
        std::string bodyStr = body.dump();
        curl_easy_setopt(curl_.get(), CURLOPT_POSTFIELDS, bodyStr.c_str());
    }

    CURLcode res = curl_easy_perform(curl_.get());
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        Logger::error("Failed to make request: " + std::string(curl_easy_strerror(res)));
        return false;
    }

    try {
        response = nlohmann::json::parse(responseStr);
        return checkResponse(response);
    } catch (const nlohmann::json::parse_error& e) {
        Logger::error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

bool VSphereRestClient::makeRequest(const std::string& method, const std::string& path, 
                                  nlohmann::json& response) {
    return makeRequest(method, path, nullptr, response);
}

bool VSphereRestClient::authenticate() {
    nlohmann::json body = {
        {"username", username_},
        {"password", password_}
    };
    nlohmann::json response;
    
    if (!makeRequest("POST", "/rest/com/vmware/cis/session", body, response)) {
        return false;
    }

    sessionId_ = response["value"];
    return true;
}

void VSphereRestClient::clearSession() {
    if (!sessionId_.empty()) {
        makeRequest("DELETE", "/rest/com/vmware/cis/session", nullptr);
        sessionId_.clear();
    }
}

std::string VSphereRestClient::getBaseUrl() const {
    return "https://" + host_ + "/api";
}

std::string VSphereRestClient::getAuthHeader() const {
    return "vmware-api-session-id: " + sessionId_;
}

void VSphereRestClient::handleError(const std::string& operation, const nlohmann::json& response) {
    std::string errorMsg = "Operation '" + operation + "' failed";
    if (response.contains("error")) {
        errorMsg += ": " + response["error"]["message"].get<std::string>();
    }
    Logger::error(errorMsg);
}

bool VSphereRestClient::checkResponse(const nlohmann::json& response) const {
    if (response.contains("error")) {
        return false;
    }
    return true;
}

size_t VSphereRestClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
} 