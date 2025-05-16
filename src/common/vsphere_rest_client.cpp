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