# Build Guide for TS (Timestamp Tool)

This document describes how to build TS on different platforms.

## Prerequisites

### Common Requirements
- GCC or Clang compiler with C11 support
- GNU Make
- Autoconf 2.69 or later
- Automake 1.15 or later
- pkg-config (optional)

### Platform-Specific Requirements

#### RHEL/CentOS/Fedora
```bash
# Install development tools
sudo dnf groupinstall "Development Tools"

# Install texinfo for documentation (required for RHEL9)
sudo dnf install texinfo --enablerepo=codeready-builder-for-rhel-9-$(arch)-rpms
```

#### Ubuntu/Debian
```bash
# Install development tools
sudo apt-get update
sudo apt-get install build-essential autoconf automake texinfo
```

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install texinfo via Homebrew (optional, for documentation)
brew install texinfo
```

## Build Process

### Standard Build
```bash
# Bootstrap the build system (recommended)
./bootstrap

# Or manually generate configure script
autoconf

# Configure the build
./configure

# Build the project
make

# Run tests
make test

# Install (optional)
sudo make install
```

### Build Options

#### Disable Documentation
If you don't have texinfo installed or don't need documentation:
```bash
./configure --disable-docs
```

#### Custom Installation Directory
```bash
./configure --prefix=/usr/local
```

#### Debug Build
```bash
make debug
```

#### Release Build
```bash
make release
```

## Troubleshooting

### Common Issues

#### "makeinfo: command not found" on RHEL9
This is a common issue on RHEL9 where texinfo is not in the default repositories. Install it from the CodeReady Builder repository:
```bash
sudo dnf install texinfo --enablerepo=codeready-builder-for-rhel-9-$(arch)-rpms
```


## Cross-Platform Compatibility

The project is designed to work on:
- Linux (RHEL, Ubuntu, Debian, etc.)
- macOS
- Other POSIX-compliant systems (untested)

The build system automatically detects platform-specific features and adjusts accordingly.

## Comprehensive Testing

After building, you can also run the tests like:
```bash
make test
```

This will run a comprehensive set of tests to verify the functionality works correctly on your platform.  `make check` runs this same test but then only summarizes pass/fail.
