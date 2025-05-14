# VMware Backup and Restore Tool

A C++ tool for backing up and restoring VMware virtual machines using the VMware VDDK (Virtual Disk Development Kit).

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

### Required System Packages
- Ubuntu 22.04 or later
- CMake 3.22 or later
- GCC 11.4.0 or later
- Build essentials
- libcurl4-openssl-dev
- nlohmann-json3-dev

### Required Libraries
1. **VMware VDDK**
   - Download VDDK from VMware's website (requires VMware account)
   - Extract to `/usr/local/vddk` or set `VDDK_ROOT` environment variable
   - Required files:
     - `/usr/local/vddk/include/vixDiskLib.h`
     - `/usr/local/vddk/lib64/libvixDiskLib.so`

## Installation

### 1. Install System Dependencies
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libcurl4-openssl-dev \
    nlohmann-json3-dev
```

### 2. Install VDDK
1. Download VDDK from VMware's website
2. Extract to `/usr/local/vddk`:
```bash
sudo mkdir -p /usr/local/vddk
sudo tar xvf vddk-*.tar.gz -C /usr/local/vddk
```

### 3. Build the Project
```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make

# Optional: Install
sudo make install
```

## Project Structure

```
.
├── include/                 # Header files
│   ├── backup/             # Backup-related headers
│   ├── common/             # Common utilities
│   └── restore/            # Restore-related headers
├── src/                    # Source files
│   ├── backup/            # Backup implementation
│   ├── common/            # Common utilities implementation
│   ├── main/              # Main executables
│   └── restore/           # Restore implementation
├── CMakeLists.txt         # CMake build configuration
└── README.md              # This file
```

## Usage

### Backup a VM
```bash
./vmware-backup \
    --host <vcenter-host> \
    --username <username> \
    --password <password> \
    --vm-name <vm-name> \
    --backup-dir <directory> \
    [--incremental] \
    [--schedule <time>] \
    [--interval <seconds>] \
    [--parallel <num>]
```

### Restore a VM
```bash
./vmware-restore \
    --host <vcenter-host> \
    --username <username> \
    --password <password> \
    --vm-name <vm-name> \
    --backup-dir <directory> \
    --datastore <datastore> \
    --resource-pool <resource-pool>
```

## Troubleshooting

### Common Build Issues

1. **VDDK Not Found**
   - Ensure VDDK is installed in `/usr/local/vddk`
   - Or set `VDDK_ROOT` environment variable to VDDK installation path
   - Verify required files exist:
     ```bash
     ls /usr/local/vddk/include/vixDiskLib.h
     ls /usr/local/vddk/lib64/libvixDiskLib.so
     ```

2. **Library Conflicts**
   - If you see warnings about library conflicts between `/usr/lib/x86_64-linux-gnu` and `/usr/local/vddk/lib64`:
     - This is expected and usually not a problem
     - The VDDK libraries will take precedence

3. **C++ Standard Library Issues**
   - Ensure you're using GCC 11.4.0 or later
   - The project requires C++17 support

### Runtime Issues

1. **Connection Failures**
   - Verify vCenter host is reachable
   - Check credentials
   - Ensure network allows VDDK connections

2. **Permission Issues**
   - Ensure user has necessary permissions on vCenter
   - Check file permissions in backup/restore directories

## Development

### Adding New Features
1. Add header files in `include/`
2. Add implementation in `src/`
3. Update `CMakeLists.txt` if adding new source files
4. Rebuild using CMake

### Code Style
- Follow the existing code style
- Use C++17 features
- Include error handling
- Add logging using the `Logger` class

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Acknowledgments

- VMware VDDK for providing the virtual disk API
- OpenSSL for cryptographic functions
- CMake for build system support
