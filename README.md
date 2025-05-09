# VMware Backup/Restore with CBT

A C++ implementation of a VMware backup and restore solution using VDDK (Virtual Disk Development Kit) and CBT (Changed Block Tracking) APIs. This project provides both scheduled and parallel backup capabilities for VMware virtual machines.

## Features

- Full VM backup
- Incremental backup using Changed Block Tracking (CBT)
- VM restore functionality
- Support for multiple disk formats
- Progress tracking and logging
- Scheduled backups (one-time and periodic)
- Parallel backup of multiple disks
- Configurable number of concurrent tasks

## Prerequisites

- VMware vSphere environment
- VDDK (Virtual Disk Development Kit) 7.0 or later
- C++17 or later
- CMake 3.10 or later
- OpenSSL development libraries

## Building the Project

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
make
```

## Usage

### Immediate Backup

```bash
./vmware-backup --host <vcenter-host> \
                --username <username> \
                --password <password> \
                --vm-name <vm-name> \
                --backup-dir <backup-directory> \
                --incremental
```

### Scheduled Backup

```bash
./vmware-backup --host <vcenter-host> \
                --username <username> \
                --password <password> \
                --vm-name <vm-name> \
                --backup-dir <backup-directory> \
                --schedule "2024-03-20 02:00:00" \
                --incremental
```

### Periodic Backup

```bash
./vmware-backup --host <vcenter-host> \
                --username <username> \
                --password <password> \
                --vm-name <vm-name> \
                --backup-dir <backup-directory> \
                --interval 86400 \
                --incremental
```

### Parallel Processing

```bash
./vmware-backup --host <vcenter-host> \
                --username <username> \
                --password <password> \
                --vm-name <vm-name> \
                --backup-dir <backup-directory> \
                --parallel 8 \
                --incremental
```

## Project Structure

```
.
├── include/
│   ├── backup/           # Backup-related headers
│   ├── restore/          # Restore-related headers
│   └── common/           # Common utilities and interfaces
├── src/
│   ├── backup/           # Backup implementation
│   ├── restore/          # Restore implementation
│   ├── common/           # Common utilities implementation
│   └── main/             # Main executables
├── tests/                # Unit tests
├── examples/             # Example usage
├── CMakeLists.txt        # Main CMake configuration
└── README.md            # This file
```

## Components

### Scheduler
- Handles task scheduling and execution
- Supports one-time and periodic tasks
- Thread-safe task management
- Error handling and recovery

### Parallel Task Manager
- Manages concurrent task execution
- Configurable number of worker threads
- Task queue management
- Progress tracking

### Backup Manager
- Coordinates backup operations
- Manages VM snapshots
- Handles CBT (Changed Block Tracking)
- Integrates scheduler and parallel processing

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- VMware VDDK for providing the virtual disk API
- OpenSSL for cryptographic functions
- CMake for build system support
