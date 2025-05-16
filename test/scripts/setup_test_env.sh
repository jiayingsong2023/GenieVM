#!/bin/bash

# Exit on error
set -e

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$TEST_DIR")"

# Configuration
CONFIG_FILE="$TEST_DIR/config/test_config.json"
LOG_DIR="$TEST_DIR/logs"
BACKUP_DIR="$TEST_DIR/backups"
TEST_VMS_FILE="$TEST_DIR/config/test_vms.json"
VSPHERE_SDK_DIR="$PROJECT_ROOT/third_party/vsphere-sdk"

# Function to log messages
log() {
    local level=$1
    local message=$2
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$level] $message"
}

# Function to verify system requirements
verify_system_requirements() {
    log "INFO" "Verifying system requirements..."
    
    # Check required commands
    local required_commands=("jq" "curl" "openssl" "python3")
    for cmd in "${required_commands[@]}"; do
        if ! command -v "$cmd" &> /dev/null; then
            log "ERROR" "Required command not found: $cmd"
            exit 1
        fi
    done
    
    # Check Python version
    local python_version=$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))')
    if (( $(echo "$python_version < 3.6" | bc -l) )); then
        log "ERROR" "Python 3.6 or higher is required. Found version: $python_version"
        exit 1
    fi
    
    # Check available memory
    local required_memory=4096  # 4GB in MB
    local available_memory=$(free -m | awk '/^Mem:/{print $7}')
    if [ "$available_memory" -lt "$required_memory" ]; then
        log "WARNING" "Low memory warning. Required: ${required_memory}MB, Available: ${available_memory}MB"
    fi
    
    # Check disk space
    local required_space=10240  # 10GB in MB
    local available_space=$(df -m "$BACKUP_DIR" | awk 'NR==2 {print $4}')
    if [ "$available_space" -lt "$required_space" ]; then
        log "ERROR" "Insufficient storage space. Required: ${required_space}MB, Available: ${available_space}MB"
        exit 1
    fi
    
    log "INFO" "System requirements verified successfully"
}

# Function to verify vSphere SDK installation
verify_vsphere_sdk() {
    log "INFO" "Verifying vSphere SDK installation..."
    
    if [ ! -d "$VSPHERE_SDK_DIR" ]; then
        log "ERROR" "vSphere SDK directory not found: $VSPHERE_SDK_DIR"
        exit 1
    fi
    
    # Check for required SDK files
    local required_files=("pyvmomi" "vsphere-automation-sdk-python")
    for file in "${required_files[@]}"; do
        if [ ! -d "$VSPHERE_SDK_DIR/$file" ]; then
            log "ERROR" "Required SDK component not found: $file"
            exit 1
        fi
    done
    
    log "INFO" "vSphere SDK verification completed successfully"
}

# Function to verify vSphere connection
verify_vsphere_connection() {
    log "INFO" "Verifying vSphere connection..."
    
    # Read vSphere configuration
    local host=$(jq -r '.vsphere.host' "$CONFIG_FILE")
    local port=$(jq -r '.vsphere.port' "$CONFIG_FILE")
    local username=$(jq -r '.vsphere.username' "$CONFIG_FILE")
    local password=$(jq -r '.vsphere.password' "$CONFIG_FILE")
    local insecure=$(jq -r '.vsphere.insecure' "$CONFIG_FILE")
    
    # Create Python script for connection test
    local test_script=$(mktemp)
    cat > "$test_script" << EOF
from pyVim.connect import SmartConnect, Disconnect
from pyVmomi import vim
import ssl

try:
    if $insecure:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS)
        context.verify_mode = ssl.CERT_NONE
    else:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS)
        context.verify_mode = ssl.CERT_REQUIRED
        context.check_hostname = True
    
    si = SmartConnect(
        host="$host",
        port=$port,
        user="$username",
        pwd="$password",
        sslContext=context
    )
    
    # Test connection by getting server time
    server_time = si.CurrentTime()
    print(f"Connected successfully. Server time: {server_time}")
    
    Disconnect(si)
    exit(0)
except Exception as e:
    print(f"Connection failed: {str(e)}")
    exit(1)
EOF
    
    # Run connection test
    if ! python3 "$test_script"; then
        log "ERROR" "Failed to connect to vSphere. Please check your configuration."
        rm "$test_script"
        exit 1
    fi
    
    rm "$test_script"
    log "INFO" "vSphere connection verified successfully"
}

# Function to check if a VM exists in vSphere
check_vm_exists() {
    local vm_name=$1
    
    # Create Python script for VM check
    local check_script=$(mktemp)
    cat > "$check_script" << EOF
from pyVim.connect import SmartConnect, Disconnect
from pyVmomi import vim
import ssl
import json
import sys

# Read config
with open("$CONFIG_FILE", 'r') as f:
    config = json.load(f)

# Connect to vSphere
if config['vsphere']['insecure']:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS)
    context.verify_mode = ssl.CERT_NONE
else:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS)
    context.verify_mode = ssl.CERT_REQUIRED
    context.check_hostname = True

si = SmartConnect(
    host=config['vsphere']['host'],
    port=config['vsphere']['port'],
    user=config['vsphere']['username'],
    pwd=config['vsphere']['password'],
    sslContext=context
)

# Search for VM
content = si.RetrieveContent()
container = content.rootFolder
view_type = [vim.VirtualMachine]
recursive = True
container_view = content.viewManager.CreateContainerView(container, view_type, recursive)
vms = container_view.view

vm_exists = False
for vm in vms:
    if vm.name == "$vm_name":
        vm_exists = True
        break

Disconnect(si)
sys.exit(0 if vm_exists else 1)
EOF
    
    # Run VM check
    if python3 "$check_script"; then
        rm "$check_script"
        return 0
    else
        rm "$check_script"
        return 1
    fi
}

# Function to create test VM
create_test_vm() {
    local vm_name=$1
    local vm_config=$2
    
    log "INFO" "Creating test VM: $vm_name"
    
    # Create Python script for VM creation
    local create_script=$(mktemp)
    cat > "$create_script" << EOF
from pyVim.connect import SmartConnect, Disconnect
from pyVmomi import vim
import ssl
import json
import time

# Read config
with open("$CONFIG_FILE", 'r') as f:
    config = json.load(f)

# Connect to vSphere
if config['vsphere']['insecure']:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS)
    context.verify_mode = ssl.CERT_NONE
else:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS)
    context.verify_mode = ssl.CERT_REQUIRED
    context.check_hostname = True

si = SmartConnect(
    host=config['vsphere']['host'],
    port=config['vsphere']['port'],
    user=config['vsphere']['username'],
    pwd=config['vsphere']['password'],
    sslContext=context
)

# Get datacenter and resource pool
content = si.RetrieveContent()
datacenter = content.rootFolder.childEntity[0]
resource_pool = datacenter.hostFolder.childEntity[0].resourcePool

# Create VM configuration
vm_config = vim.vm.ConfigSpec(
    name="$vm_name",
    memoryMB=1024,
    numCPUs=1,
    files=vim.vm.FileInfo(
        vmPathName="[datastore1] $vm_name/$vm_name.vmx"
    )
)

# Create VM
task = resource_pool.CreateVM(
    config=vm_config,
    pool=resource_pool,
    host=datacenter.hostFolder.childEntity[0]
)

# Wait for task to complete
while task.info.state not in [vim.TaskInfo.State.success, vim.TaskInfo.State.error]:
    time.sleep(1)

if task.info.state == vim.TaskInfo.State.error:
    raise Exception(f"Failed to create VM: {task.info.error.msg}")

Disconnect(si)
EOF
    
    # Run VM creation
    if ! python3 "$create_script"; then
        log "ERROR" "Failed to create test VM: $vm_name"
        rm "$create_script"
        exit 1
    fi
    
    rm "$create_script"
    log "INFO" "Test VM created successfully: $vm_name"
}

# Function to remove test VM
remove_test_vm() {
    local vm_name=$1
    
    log "INFO" "Removing test VM: $vm_name"
    
    # Create Python script for VM removal
    local remove_script=$(mktemp)
    cat > "$remove_script" << EOF
from pyVim.connect import SmartConnect, Disconnect
from pyVmomi import vim
import ssl
import json
import time

# Read config
with open("$CONFIG_FILE", 'r') as f:
    config = json.load(f)

# Connect to vSphere
if config['vsphere']['insecure']:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS)
    context.verify_mode = ssl.CERT_NONE
else:
    context = ssl.SSLContext(ssl.PROTOCOL_TLS)
    context.verify_mode = ssl.CERT_REQUIRED
    context.check_hostname = True

si = SmartConnect(
    host=config['vsphere']['host'],
    port=config['vsphere']['port'],
    user=config['vsphere']['username'],
    pwd=config['vsphere']['password'],
    sslContext=context
)

# Search for VM
content = si.RetrieveContent()
container = content.rootFolder
view_type = [vim.VirtualMachine]
recursive = True
container_view = content.viewManager.CreateContainerView(container, view_type, recursive)
vms = container_view.view

target_vm = None
for vm in vms:
    if vm.name == "$vm_name":
        target_vm = vm
        break

if target_vm:
    # Power off VM if it's running
    if target_vm.runtime.powerState == vim.VirtualMachinePowerState.poweredOn:
        log("INFO", "Powering off VM before removal...")
        task = target_vm.PowerOffVM_Task()
        while task.info.state not in [vim.TaskInfo.State.success, vim.TaskInfo.State.error]:
            time.sleep(1)
    
    # Remove VM
    log("INFO", "Removing VM...")
    task = target_vm.Destroy_Task()
    while task.info.state not in [vim.TaskInfo.State.success, vim.TaskInfo.State.error]:
        time.sleep(1)
    
    if task.info.state == vim.TaskInfo.State.success:
        log("INFO", "VM removed successfully")
    else:
        log("ERROR", f"Failed to remove VM: {task.info.error.msg}")
        sys.exit(1)
else:
    log("WARNING", "VM not found: $vm_name")

Disconnect(si)
EOF
    
    # Run VM removal
    if ! python3 "$remove_script"; then
        log "ERROR" "Failed to remove VM: $vm_name"
        rm "$remove_script"
        exit 1
    fi
    
    rm "$remove_script"
    log "INFO" "VM removal completed successfully"
}

# Function to setup test environment
setup_test_environment() {
    log "INFO" "Setting up test environment..."
    
    # Verify system requirements
    verify_system_requirements
    
    # Verify vSphere SDK
    verify_vsphere_sdk
    
    # Create necessary directories
    mkdir -p "$LOG_DIR"
    mkdir -p "$BACKUP_DIR"
    
    # Verify vSphere connection
    verify_vsphere_connection
    
    # Create test VMs if they don't exist
    if [ -f "$TEST_VMS_FILE" ]; then
        while IFS= read -r vm_config; do
            local vm_name=$(echo "$vm_config" | jq -r '.name')
            if ! check_vm_exists "$vm_name"; then
                create_test_vm "$vm_name" "$vm_config"
            else
                log "INFO" "Test VM already exists: $vm_name"
            fi
        done < <(jq -c '.test_vms[]' "$CONFIG_FILE")
    else
        log "WARNING" "Test VMs configuration file not found: $TEST_VMS_FILE"
    fi
    
    log "INFO" "Test environment setup completed successfully"
}

# Function to cleanup test environment
cleanup_test_environment() {
    log "INFO" "Cleaning up test environment..."
    
    # Remove old backup files
    find "$BACKUP_DIR" -type f -mtime +7 -delete
    
    # Archive old logs
    find "$LOG_DIR" -type f -mtime +7 -exec mv {} "$LOG_DIR/archive/" \;
    
    # Clean up test VMs (optional)
    if [ "$1" == "--remove-vms" ]; then
        log "INFO" "Removing test VMs..."
        # TODO: Implement VM removal using vSphere API
    fi
    
    log "INFO" "Test environment cleanup completed"
}

# Main execution
log "INFO" "Starting test environment setup..."

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --cleanup)
            cleanup_test_environment "$2"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
    shift
done

# Setup test environment
setup_test_environment

log "INFO" "Test environment is ready for testing" 