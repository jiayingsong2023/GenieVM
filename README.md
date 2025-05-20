# VM Backup and Restore Tool (VMware & KVM)

A C++ tool for backing up and restoring virtual machines on both VMware (using VDDK) and KVM (using libvirt, qemu, and LVM).

## Features

- Full VM backup and restore (VMware & KVM)
- Incremental backup using Changed Block Tracking (CBT) for both platforms
- KVM support: QCOW2 and LVM disk types
- VMware support: VDDK-based backup/restore
- Progress tracking and logging
- Scheduled backups (one-time and periodic)
- Parallel backup of multiple disks
- Configurable number of concurrent tasks
- Modern CLI

## Prerequisites

### Required System Packages
- Ubuntu 22.04 or later
- CMake 3.22 or later
- GCC 11.4.0 or later
- build-essential
- libcurl4-openssl-dev
- nlohmann-json3-dev
- **libvirt-dev** (for KVM)
- **qemu-utils** (for KVM, QCOW2 support)
- **lvm2** (for KVM, LVM support)
- **libssl-dev** (for SSL support)
- **libcrypto++-dev** (for cryptographic operations)
- **zlib1g-dev** (for compression)
- **googletest** (for unit testing)

### Required Libraries
1. **VMware VDDK** (for VMware support)
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
    nlohmann-json3-dev \
    libvirt-dev \
    qemu-utils \
    lvm2 \
    libssl-dev \
    libcrypto++-dev \
    zlib1g-dev \
    googletest
```

### 2. Install VDDK (VMware only)
1. Download VDDK from VMware's website
2. Extract to `/usr/local/vddk`:
```bash
sudo mkdir -p /usr/local/vddk
sudo tar xvf vddk-*.tar.gz -C /usr/local/vddk
```

### 3. Build the Project
```bash
# From project root
git submodule update --init --recursive # if needed
./build.sh rebuild
```

## Project Structure

```
.
├── include/                 # Header files
│   ├── backup/             # Backup-related headers
│   ├── common/             # Common utilities
│   └── restore/            # Restore-related headers
├── src/                    # Source files
│   ├── backup/            # Backup implementation (KVM & VMware)
│   ├── common/            # Common utilities implementation
│   ├── main/              # Main executables
│   └── restore/           # Restore implementation
├── CMakeLists.txt         # CMake build configuration
├── build.sh               # Build helper script
└── README.md              # This file
```

## Usage

### General CLI
The CLI supports both VMware and KVM. Use the `--type` flag to specify the platform.

#### Backup a VM
```bash
./genievm backup \
    -v, --vm-name <name>       Name of the VM to backup \
    -b, --backup-dir <dir>     Directory for backup \
    -s, --server <host>        vCenter/ESXi/KVM host \
    -u, --username <user>      Username for host \
    -p, --password <pass>      Password for host \
    -i, --incremental          Use incremental backup (CBT) \
    --schedule <time>          Schedule backup at specific time (YYYY-MM-DD HH:MM:SS) \
    --interval <seconds>       Schedule periodic backup every N seconds \
    --parallel <num>           Number of parallel backup tasks (default: 4) \
    --compression <level>      Compression level (0-9, default: 0) \
    --retention <days>         Number of days to keep backups (default: 7) \
    --max-backups <num>        Maximum number of backups to keep (default: 10) \
    --disable-cbt              Disable Changed Block Tracking \
    --exclude-disk <path>      Exclude disk from backup (can be used multiple times)
```

#### Restore a VM
```bash
./genievm restore \
    -v, --vm-name <name>       Name of the VM to restore \
    -b, --backup-dir <dir>     Directory containing the backup \
    -d, --datastore <name>     Target datastore for restore \
    -r, --resource-pool <name> Target resource pool for restore \
    -s, --server <host>        vCenter/ESXi/KVM host \
    -u, --username <user>      Username for host \
    -p, --password <pass>      Password for host
```

#### Additional Commands
```bash
./genievm schedule    # Schedule a backup
./genievm list        # List backups
./genievm verify      # Verify a backup
```

#### KVM-specific Notes
- For KVM, `--host` is the KVM/libvirt host (e.g., localhost or remote IP)
- Requires libvirt and qemu to be installed and running
- Supports both QCOW2 and LVM disks
- CBT for QCOW2 uses QEMU dirty bitmaps; for LVM uses LVM snapshots

#### VMware-specific Notes
- For VMware, `--host` is the vCenter/ESXi host
- Requires VDDK and vCenter/ESXi credentials

## Troubleshooting

### Common Build Issues

1. **VDDK Not Found (VMware only)**
   - Ensure VDDK is installed in `/usr/local/vddk`
   - Or set `VDDK_ROOT` environment variable to VDDK installation path
   - Verify required files exist:
     ```bash
     ls /usr/local/vddk/include/vixDiskLib.h
     ls /usr/local/vddk/lib64/libvixDiskLib.so
     ```

2. **libvirt or qemu-img Not Found (KVM only)**
   - Install with: `sudo apt install libvirt-dev qemu-utils lvm2`

3. **C++ Standard Library Issues**
   - Ensure you're using GCC 11.4.0 or later
   - The project requires C++17 support

### Runtime Issues

1. **Connection Failures**
   - For VMware: verify vCenter/ESXi host is reachable
   - For KVM: verify libvirt is running and accessible
   - Check credentials
   - Ensure network allows required connections

2. **Permission Issues**
   - Ensure user has necessary permissions on vCenter or KVM host
   - Check file permissions in backup/restore directories

## Development

### Adding New Features
1. Add header files in `include/`
2. Add implementation in `src/`
3. Update `CMakeLists.txt` if adding new source files
4. Rebuild using CMake or `./build.sh rebuild`

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
- libvirt, qemu, and LVM for KVM support
- OpenSSL for cryptographic functions
- CMake for build system support
