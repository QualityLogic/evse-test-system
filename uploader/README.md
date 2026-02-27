# Everest Test System

Installer and manager for deploying a custom Everest firmware stack onto an EV charger running Linux.

The charger ships with an existing Everest firmware stack managed by a systemd service called `basecamp`. This tooling installs a custom Everest distribution **alongside** the original, redirects the systemd service to the custom installation, and provides commands to uninstall or modify it. The original firmware is never modified and can be restored at any time.

## Project Structure

```
everest-test-system/
├── upload.sh                        # Copies the installer to the remote charger
├── settings.env                     # Upload settings (remote host, user, paths)
├── README.md
└── installer/
    ├── everest.sh                   # Install, uninstall, and modify on the charger
    ├── settings.env                 # Installation settings (paths, config, service)
    ├── bin/
    │   └── runtime.tar.gz           # Compressed custom Everest distribution
    └── services/
        └── basecamp.service         # Systemd service template
```

## Quick Start

### 1. Configure Upload Settings

Edit `settings.env` in the project root to set the remote charger connection details:

```sh
REMOTE_HOST="192.168.3.11"     # Charger IP address or hostname
REMOTE_USER="root"              # SSH user
REMOTE_INSTALLER_PATH="~/everest_installer"  # Destination on the charger
LOCAL_INSTALLER_DIR="installer"  # Local directory to upload
```

### 2. Configure Installation Settings

Edit `installer/settings.env` to customize the installation:

```sh
EVEREST_CONFIG_FILE="config-phytec-ac-ocpp-pnc-test.yaml"  # Config file to use
EVEREST_INSTALL_DIR="/var/ev-test-sys"   # Where to install on the charger
EVEREST_ARCHIVE_FILE="bin/runtime.tar.gz"  # Path to the firmware archive
EVEREST_SERVICE_DIR="/lib/systemd/system"  # Systemd service directory
EVEREST_SERVICE="basecamp"       # Name of the systemd service
```

### 3. Upload to the Charger

From your local machine, upload the installer directory to the charger:

```sh
./upload.sh
```

This will verify the remote host is reachable, display the approximate transfer size, and ask for confirmation before copying files via SCP.

### 4. Install on the Charger

SSH into the charger and run the installer:

```sh
ssh root@192.168.3.11
cd ~/everest_installer
./everest.sh install
```

The installer will:

1. Stop the running `basecamp` service (if active)
2. Create the installation directory
3. Extract the firmware archive
4. Back up the original systemd service file
5. Update the systemd service to point to the new installation
6. Reload the systemd daemon

If anything goes wrong during installation, all changes are automatically rolled back.

### 5. Start the Service

After a successful installation, start the service manually:

```sh
systemctl start basecamp
```

## Commands

### upload.sh

Run from your local machine to transfer installer files to the charger.

| Command | Description |
|---------|-------------|
| `./upload.sh` | Copy the installer directory to the remote system |
| `./upload.sh --help` | Show help |

### everest.sh

Run on the charger to manage the custom Everest installation.

| Command | Description |
|---------|-------------|
| `./everest.sh install` | Install the custom Everest firmware |
| `./everest.sh uninstall` | Remove the custom firmware and restore the original |
| `./everest.sh modify` | Update the systemd service to use a different config file |
| `./everest.sh --help` | Show help |

## Detailed Command Reference

### Install

```sh
./everest.sh install
```

Installs the custom Everest firmware. The installer checks that no existing custom installation is present and that the `basecamp` systemd service exists. After extracting the firmware archive, it creates a manifest file (`.everest_manifest`) in the installation directory to track exactly which files were installed. The original systemd service file is backed up before being replaced.

If the config file specified in `settings.env` is not found in the extracted archive, a warning is displayed with instructions to use the `modify` command to correct it.

### Uninstall

```sh
./everest.sh uninstall
```

Removes the custom Everest firmware and restores the original systemd service. Only files tracked in the installation manifest are removed, so pre-existing files at the installation path are never touched. The installation directory itself is only removed if it is empty after cleanup.

### Modify

```sh
./everest.sh modify
```

Updates the systemd service to use a different config file. Edit `EVEREST_CONFIG_FILE` in `installer/settings.env` before running this command. If the specified config file does not exist in the installation, a warning is shown and confirmation is required before proceeding.

## Safety Features

- **Manifest-based tracking**: Installation creates a `.everest_manifest` file that records every file extracted from the archive. Uninstallation only removes files listed in the manifest rather than performing a blanket directory removal.
- **Automatic rollback**: If an error occurs during installation, all completed steps are reversed automatically.
- **Service backup**: The original systemd service file is backed up before modification and restored during uninstall.
- **User confirmation**: Every destructive operation displays a summary of planned actions and requires explicit confirmation before proceeding.
