# GenieVM Backup/Restore System Design

## Overview

GenieVM is a multi-hypervisor backup and restore system that supports both VMware vSphere and KVM environments. The system provides a unified interface for backing up and restoring virtual machines across different hypervisor platforms.

## System Architecture

### High-Level Components

The system consists of several key components:

1. **Command Line Interface (main.cpp)** - Entry point for user interaction
2. **BackupCLI (backup_cli.cpp)** - High-level command processing
3. **JobManager (job_manager.cpp)** - Central job coordination
4. **BackupProvider Interface** - Abstract hypervisor interface
5. **ParallelTaskManager** - Concurrent disk operations
6. **VDDK Wrapper** - VMware disk operations
7. **Libvirt Integration** - KVM operations

### Component Details

#### 1. Command Line Interface (main.cpp)
- **Purpose**: Entry point for user interaction and command processing
- **Key Features**:
  - Command parsing and validation
  - Help and version information
  - Logger initialization
  - Error handling

#### 2. BackupCLI (backup_cli.cpp)
- **Purpose**: High-level command processing and user interface
- **Key Features**:
  - Provider initialization and management
  - Job scheduling and coordination
  - Progress reporting
  - Configuration management

#### 3. JobManager (job_manager.cpp)
- **Purpose**: Central coordination of backup/restore operations
- **Key Features**:
  - Job lifecycle management
  - Provider abstraction
  - Job registry and tracking
  - Error handling and recovery

#### 4. BackupProvider Interface
- **Purpose**: Abstract interface for hypervisor-specific operations
- **Key Features**:
  - Connection management
  - VM operations (snapshots, disk paths)
  - Backup/restore operations
  - Changed Block Tracking (CBT)
  - Progress tracking
- **Implementations**:
  - VMwareBackupProvider
  - KVMBackupProvider

## Hypervisor Support

### VMware vSphere Support

#### VMwareBackupProvider
- **Purpose**: VMware-specific backup and restore operations
- **Key Features**:
  - vSphere REST API integration
  - VDDK wrapper integration for disk operations
  - Changed Block Tracking (CBT) support
  - Snapshot management
  - Backup metadata management

#### VDDK Wrapper (vddk_wrapper.cpp)
- **Purpose**: C++ wrapper around VMware VDDK API
- **Key Features**:
  - VDDK initialization and cleanup
  - Disk connection management
  - Block-level read/write operations
  - Changed Block Tracking operations
  - Error handling and translation

#### vSphere REST Client (vsphere_rest_client.cpp)
- **Purpose**: REST API client for vSphere operations
- **Key Features**:
  - VM management operations
  - Snapshot creation and removal
  - CBT enable/disable
  - VM information retrieval

### KVM Support

#### KVMBackupProvider
- **Purpose**: KVM-specific backup and restore operations
- **Key Features**:
  - Libvirt integration
  - QCOW2 format support
  - LVM support
  - Snapshot management
  - CBT factory integration

#### CBT Factory (cbt_factory.cpp)
- **Purpose**: Factory for creating CBT providers
- **Key Features**:
  - QCOW2 CBT provider creation
  - LVM CBT provider creation
  - Storage type detection
  - CBT provider selection

## Job System

### Job Base Class (job.cpp)
- **Purpose**: Base class for all job types
- **Key Features**:
  - Job lifecycle management (start, pause, resume, cancel)
  - Progress tracking
  - Status management
  - Error handling
  - Unique ID generation

### BackupJob (backup_job.cpp)
- **Purpose**: Manages backup operations
- **Key Features**:
  - Snapshot creation and cleanup
  - Multi-disk backup coordination
  - Backup metadata management
  - Verification support
  - Progress tracking per disk

### RestoreJob (restore_job.cpp)
- **Purpose**: Manages restore operations
- **Key Features**:
  - Multi-disk restore coordination
  - Backup metadata validation
  - Restore verification
  - Progress tracking per disk

### VerifyJob (verify_job.cpp)
- **Purpose**: Manages backup verification operations
- **Key Features**:
  - Backup integrity verification
  - Checksum validation
  - Metadata validation
  - Progress tracking

## Configuration Management

### BackupConfig
```cpp
struct BackupConfig {
    std::string vmId;
    std::string backupPath;
    std::string backupDir;
    bool incremental;
    int compressionLevel;
    int maxConcurrentDisks;
    int retentionDays;
    int maxBackups;
    bool enableCBT;
    std::vector<std::string> excludedDisks;
};
```

### RestoreConfig
```cpp
struct RestoreConfig {
    std::string vmId;
    std::string backupId;
    std::string restorePath;
    bool verifyAfterRestore;
    bool powerOnAfterRestore;
    std::vector<DiskConfig> diskConfigs;
};
```

## Backup Workflow

### 1. Command Line to Backup Flow

1. User executes backup command
2. BackupCLI initializes provider and job manager
3. JobManager creates backup job
4. BackupProvider creates snapshot
5. BackupJob coordinates multi-disk backup
6. ParallelTaskManager executes disk operations
7. Backup metadata is saved
8. Snapshot is removed
9. Job completion is reported

### 2. Multi-Disk Backup Process

1. Start backup operation
2. Create VM snapshot
3. Get all disk paths for VM
4. Initialize parallel task manager
5. Submit disk backup tasks
6. Process disks in parallel
7. Wait for all disk operations to complete
8. Save backup metadata
9. Remove snapshot
10. Complete backup

## Restore Workflow

### 1. Command Line to Restore Flow

1. User executes restore command
2. BackupCLI initializes provider and job manager
3. JobManager creates restore job
4. BackupProvider validates backup
5. RestoreJob coordinates multi-disk restore
6. ParallelTaskManager executes disk operations
7. Restore verification is performed
8. Job completion is reported

### 2. Multi-Disk Restore Process

1. Start restore operation
2. Load backup metadata
3. Validate backup integrity
4. Initialize parallel task manager
5. Submit disk restore tasks
6. Process disks in parallel
7. Wait for all disk operations to complete
8. Verify restore integrity
9. Complete restore

## Changed Block Tracking (CBT)

### VMware CBT
- **Implementation**: VDDK CBT API integration
- **Features**:
  - Block-level change tracking
  - Incremental backup support
  - Change ID management
  - Automatic CBT enable/disable

### KVM CBT
- **Implementation**: QCOW2 and LVM CBT providers
- **Features**:
  - QCOW2 internal snapshots
  - LVM volume change tracking
  - Storage type detection
  - Provider factory pattern

## Error Handling

### Error Categories
1. **Connection Errors**: Network, authentication, timeout
2. **Disk Operation Errors**: I/O failures, space issues
3. **API Errors**: VDDK, libvirt, vSphere REST API
4. **Resource Errors**: Memory, disk space, permissions
5. **Validation Errors**: Configuration, parameter validation

### Recovery Strategies
- **Automatic Retries**: For transient network and I/O errors
- **Fallback Operations**: Full backup on incremental failure
- **Cleanup**: Automatic snapshot and temporary file cleanup
- **State Recovery**: Resume interrupted operations

## Performance Considerations

### Resource Management
- **Thread Pool**: Configurable worker thread count
- **Memory Usage**: Optimized buffer management
- **Disk I/O**: Parallel disk operations
- **Network**: Bandwidth control and optimization

### Optimization Techniques
- **Parallel Processing**: Multi-disk concurrent operations
- **Incremental Backups**: Changed Block Tracking support
- **Compression**: Configurable compression levels
- **Buffering**: Optimized I/O buffer sizes

## Security

### Authentication
- **vSphere**: Username/password authentication
- **KVM**: SSH key or password authentication
- **Credential Management**: Secure credential handling

### Data Protection
- **Encrypted Communication**: SSL/TLS for all network operations
- **Secure Storage**: Encrypted backup storage support
- **Access Control**: File system permissions and validation

## Build System

### CMake Configuration
- **Dependencies**:
  - VDDK libraries (VMware)
  - Libvirt (KVM)
  - libcurl (REST API)
  - nlohmann/json (JSON processing)
  - OpenSSL (Cryptography)
  - GTest (Testing)

### VDDK Integration
- **Wrapper Library**: C++ wrapper around VDDK C API
- **ABI Compatibility**: Old ABI for VDDK compatibility
- **Library Loading**: Dynamic loading of VDDK libraries
- **Error Handling**: Comprehensive error translation

## Testing

### Unit Tests
- **Framework**: Google Test
- **Coverage**: Core components and utilities
- **Mocking**: Provider and API mocking

### Integration Tests
- **Environment**: Test VM setup scripts
- **Scenarios**: Backup, restore, and verification workflows
- **Validation**: End-to-end operation verification

## Deployment

### Package Management
- **Debian Package**: Automated build and packaging
- **Dependencies**: Runtime dependency management
- **Installation**: Standard package installation process

### Configuration
- **Environment Variables**: Runtime configuration
- **Configuration Files**: JSON-based configuration
- **Logging**: Configurable log levels and output

## Future Enhancements

### Planned Features
1. **Additional Hypervisors**: Hyper-V, Xen support
2. **Cloud Integration**: AWS, Azure, GCP backup
3. **Advanced Scheduling**: Complex backup schedules
4. **Monitoring**: Real-time backup monitoring
5. **API**: REST API for integration
6. **Web UI**: Web-based management interface

### Architecture Improvements
1. **Plugin System**: Extensible provider architecture
2. **Distributed Processing**: Multi-node backup processing
3. **Deduplication**: Storage deduplication support
4. **Encryption**: End-to-end encryption
5. **Compression**: Advanced compression algorithms
