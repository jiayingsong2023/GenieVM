# VSphereManager Documentation

## Overview
The `VSphereManager` class provides a high-level interface for managing VMware vSphere resources using REST API calls. It replaces the previous implementation that used the vSphere Management SDK, making it more lightweight and easier to maintain.

## REST API Endpoints

### Authentication
- **POST** `/rest/com/vmware/cis/session`
  - Creates a new session
  - Returns session ID for subsequent requests
  - Headers: Basic authentication with username/password

### VM Operations
- **POST** `/rest/vcenter/vm`
  - Creates a new VM
  - Body: VM configuration (name, datastore, resource pool)
  - Returns: VM ID

- **POST** `/rest/vcenter/vm/{vm}/disk`
  - Attaches a disk to a VM
  - Parameters: VM ID
  - Body: Disk configuration
  - Returns: Disk ID

- **GET** `/rest/vcenter/vm`
  - Lists all VMs
  - Query parameters: filter, names
  - Returns: List of VM information

### Resource Management
- **GET** `/rest/vcenter/datastore`
  - Lists all datastores
  - Query parameters: filter, names
  - Returns: List of datastore information

- **GET** `/rest/vcenter/resource-pool`
  - Lists all resource pools
  - Query parameters: filter, names
  - Returns: List of resource pool information

## Class Structure

### Constructor
```cpp
VSphereManager(const std::string& host, const std::string& username, const std::string& password)
```
- **Parameters**:
  - `host`: vSphere server hostname or IP address
  - `username`: Username for authentication
  - `password`: Password for authentication
- **Description**: Initializes the VSphereManager with connection details and creates a VSphereRestClient instance.

### Connection Management

#### connect()
```cpp
bool connect()
```
- **Returns**: `true` if connection is successful, `false` otherwise
- **Description**: Establishes a connection to the vSphere server using REST API
- **Error Handling**: Logs errors if connection fails

#### disconnect()
```cpp
void disconnect()
```
- **Description**: Closes the connection to the vSphere server
- **Error Handling**: Ensures clean disconnection and resource cleanup

### VM Operations

#### createVM()
```cpp
bool createVM(const std::string& vmName, const std::string& datastoreName, const std::string& resourcePoolName)
```
- **Parameters**:
  - `vmName`: Name of the virtual machine to create
  - `datastoreName`: Name of the datastore to store the VM
  - `resourcePoolName`: Name of the resource pool for the VM
- **Returns**: `true` if VM creation is successful, `false` otherwise
- **Description**: Creates a new virtual machine with specified parameters
- **Error Handling**: Logs errors if VM creation fails

#### attachDisks()
```cpp
bool attachDisks(const std::string& vmName, const std::vector<std::string>& diskPaths)
```
- **Parameters**:
  - `vmName`: Name of the target virtual machine
  - `diskPaths`: Vector of disk paths to attach
- **Returns**: `true` if disk attachment is successful, `false` otherwise
- **Description**: Attaches specified disks to a virtual machine
- **Error Handling**: Logs errors if disk attachment fails

### Resource Information

#### getVM()
```cpp
bool getVM(const std::string& vmName, std::string& vmId)
```
- **Parameters**:
  - `vmName`: Name of the virtual machine to find
  - `vmId`: Output parameter for the VM's ID
- **Returns**: `true` if VM is found, `false` otherwise
- **Description**: Retrieves the ID of a virtual machine by name
- **Error Handling**: Logs errors if VM lookup fails

#### getDatastore()
```cpp
bool getDatastore(const std::string& datastoreName, std::string& datastoreId)
```
- **Parameters**:
  - `datastoreName`: Name of the datastore to find
  - `datastoreId`: Output parameter for the datastore's ID
- **Returns**: `true` if datastore is found, `false` otherwise
- **Description**: Retrieves the ID of a datastore by name
- **Error Handling**: Logs errors if datastore lookup fails

#### getResourcePool()
```cpp
bool getResourcePool(const std::string& poolName, std::string& poolId)
```
- **Parameters**:
  - `poolName`: Name of the resource pool to find
  - `poolId`: Output parameter for the resource pool's ID
- **Returns**: `true` if resource pool is found, `false` otherwise
- **Description**: Retrieves the ID of a resource pool by name
- **Error Handling**: Logs errors if resource pool lookup fails

## Usage Examples

### Basic VM Creation
```cpp
// Create VSphereManager instance
VSphereManager manager("vsphere.example.com", "admin", "password");

// Connect to vSphere
if (!manager.connect()) {
    std::cerr << "Failed to connect to vSphere" << std::endl;
    return;
}

// Create a new VM
if (manager.createVM("MyVM", "datastore1", "pool1")) {
    std::cout << "VM created successfully" << std::endl;
}

// Disconnect when done
manager.disconnect();
```

### Disk Management
```cpp
// Attach multiple disks to a VM
std::vector<std::string> diskPaths = {
    "[datastore1] MyVM/disk1.vmdk",
    "[datastore1] MyVM/disk2.vmdk"
};

if (manager.attachDisks("MyVM", diskPaths)) {
    std::cout << "Disks attached successfully" << std::endl;
} else {
    std::cerr << "Failed to attach disks" << std::endl;
}
```

### Resource Information Retrieval
```cpp
// Get VM information
std::string vmId;
if (manager.getVM("MyVM", vmId)) {
    std::cout << "VM ID: " << vmId << std::endl;
    
    // Get datastore information
    std::string datastoreId;
    if (manager.getDatastore("datastore1", datastoreId)) {
        std::cout << "Datastore ID: " << datastoreId << std::endl;
    }
    
    // Get resource pool information
    std::string poolId;
    if (manager.getResourcePool("pool1", poolId)) {
        std::cout << "Resource Pool ID: " << poolId << std::endl;
    }
}
```

### Error Handling Example
```cpp
try {
    VSphereManager manager("vsphere.example.com", "admin", "password");
    
    if (!manager.connect()) {
        throw std::runtime_error("Failed to connect to vSphere");
    }
    
    // Create VM with error handling
    if (!manager.createVM("MyVM", "datastore1", "pool1")) {
        throw std::runtime_error("Failed to create VM");
    }
    
    // Attach disks with error handling
    std::vector<std::string> diskPaths = {"[datastore1] MyVM/disk1.vmdk"};
    if (!manager.attachDisks("MyVM", diskPaths)) {
        throw std::runtime_error("Failed to attach disks");
    }
    
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## Error Handling
All methods include error handling and logging:
- Connection errors are logged using the Logger class
- Method failures return `false` and log appropriate error messages
- Resource cleanup is handled in the destructor
- HTTP errors are mapped to appropriate error messages
- Network timeouts are handled with retry logic

## Dependencies
- VSphereRestClient: Handles REST API communication
- Logger: Provides logging functionality
- CURL: For HTTP requests (used by VSphereRestClient)
- JSON: For request/response parsing

## Notes
- The class uses REST API calls instead of the vSphere Management SDK
- All operations require an active connection
- Error messages are logged using the Logger class
- Resource cleanup is automatic through RAII principles
- HTTP requests are retried on network failures
- Session management is handled automatically
- All API calls are asynchronous and non-blocking 