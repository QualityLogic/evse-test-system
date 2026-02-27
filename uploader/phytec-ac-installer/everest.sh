#!/bin/bash
set -euo pipefail

# =============================================================================
# Everest Test System - Installer / Uninstaller / Modifier
# =============================================================================

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd -P)"
SETTINGS_FILE="$SCRIPT_DIR/settings.env"
SERVICE_TEMPLATE="$SCRIPT_DIR/services/basecamp.service"

# -----------------------------------------------------------------------------
# Load settings
# -----------------------------------------------------------------------------
load_settings() {
    if [[ ! -f "$SETTINGS_FILE" ]]; then
        echo "Error: Settings file not found at $SETTINGS_FILE"
        exit 1
    fi
    # shellcheck source=settings.env
    source "$SETTINGS_FILE"

    # Validate required variables
    local missing=()
    [[ -z "${EVEREST_CONFIG_FILE:-}" ]] && missing+=("EVEREST_CONFIG_FILE")
    [[ -z "${EVEREST_INSTALL_DIR:-}" ]] && missing+=("EVEREST_INSTALL_DIR")
    [[ -z "${EVEREST_ARCHIVE_FILE:-}" ]] && missing+=("EVEREST_ARCHIVE_FILE")
    [[ -z "${EVEREST_SERVICE_DIR:-}" ]] && missing+=("EVEREST_SERVICE_DIR")
    [[ -z "${EVEREST_SERVICE:-}" ]] && missing+=("EVEREST_SERVICE")

    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "Error: The following required settings are missing in $SETTINGS_FILE:"
        printf '  - %s\n' "${missing[@]}"
        exit 1
    fi

    ARCHIVE_PATH="$SCRIPT_DIR/$EVEREST_ARCHIVE_FILE"
    SERVICE_FILE="$EVEREST_SERVICE_DIR/${EVEREST_SERVICE}.service"
    SERVICE_BACKUP="$EVEREST_SERVICE_DIR/${EVEREST_SERVICE}.service.bak"
    MANIFEST_FILE="$EVEREST_INSTALL_DIR/.everest_manifest"
}

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------
confirm() {
    local prompt="$1"
    read -r -p "$prompt [y/N]: " response
    case "$response" in
        [yY][eE][sS]|[yY]) return 0 ;;
        *) return 1 ;;
    esac
}

is_service_active() {
    systemctl is-active --quiet "$EVEREST_SERVICE"
}

is_installed() {
    [[ -f "$MANIFEST_FILE" ]]
}

remove_installed_files() {
    # Remove files and symlinks first (reverse sorted so deeper paths come first)
    while IFS= read -r entry; do
        local target="$EVEREST_INSTALL_DIR/$entry"
        if [[ -f "$target" || -L "$target" ]]; then
            rm -f "$target"
        fi
    done < <(sort -r "$MANIFEST_FILE")

    # Remove directories bottom-up (only if empty, reverse sorted)
    while IFS= read -r entry; do
        local target="$EVEREST_INSTALL_DIR/$entry"
        if [[ -d "$target" ]]; then
            rmdir --ignore-fail-on-non-empty "$target" 2>/dev/null || true
        fi
    done < <(grep '/$' "$MANIFEST_FILE" | sort -r)

    # Remove the manifest itself
    rm -f "$MANIFEST_FILE"

    # Remove install dir only if it's now empty
    rmdir --ignore-fail-on-non-empty "$EVEREST_INSTALL_DIR" 2>/dev/null || true
}

# -----------------------------------------------------------------------------
# Install
# -----------------------------------------------------------------------------
do_install() {
    echo "=== Everest Install ==="
    echo

    # Check if already installed
    if is_installed; then
        echo "Error: An existing Everest installation was found at $EVEREST_INSTALL_DIR."
        echo "Please run './everest.sh uninstall' before installing a new version."
        exit 1
    fi

    # Verify the systemd service exists
    if [[ ! -f "$SERVICE_FILE" ]]; then
        echo "Error: Systemd service '$EVEREST_SERVICE' not found at $SERVICE_FILE."
        echo "Cannot proceed without an existing service file."
        exit 1
    fi

    # Verify the archive exists
    if [[ ! -f "$ARCHIVE_PATH" ]]; then
        echo "Error: Archive file not found at $ARCHIVE_PATH."
        exit 1
    fi

    # Verify the service template exists
    if [[ ! -f "$SERVICE_TEMPLATE" ]]; then
        echo "Error: Service template not found at $SERVICE_TEMPLATE."
        exit 1
    fi

    # Check if service is running
    local service_was_running=false
    if is_service_active; then
        service_was_running=true
    fi

    # Inform user of planned actions
    local step=1
    echo "The following actions will be performed:"
    if $service_was_running; then
        echo "  $step. Stop the '$EVEREST_SERVICE' systemd service"
        ((step++))
    fi
    echo "  $step. Create installation directory: $EVEREST_INSTALL_DIR"
    ((step++))
    echo "  $step. Extract firmware archive to: $EVEREST_INSTALL_DIR"
    ((step++))
    echo "  $step. Backup the original systemd service to: $SERVICE_BACKUP"
    ((step++))
    echo "  $step. Update the systemd service to point to the new installation"
    echo "     - Install dir : $EVEREST_INSTALL_DIR"
    echo "     - Config file : $EVEREST_CONFIG_FILE"
    ((step++))
    echo "  $step. Reload the systemd daemon"
    echo

    if ! confirm "Do you wish to proceed?"; then
        echo "Installation cancelled. No changes have been made."
        exit 0
    fi

    echo

    # Track what we've done for rollback
    local stopped_service=false
    local created_dir=false
    local extracted_archive=false
    local backed_up_service=false
    local overwrote_service=false

    rollback() {
        trap - INT TERM EXIT

        echo
        echo "Error encountered. Rolling back changes..."

        if $overwrote_service && $backed_up_service; then
            echo "  Restoring original systemd service..."
            cp "$SERVICE_BACKUP" "$SERVICE_FILE" 2>/dev/null || true
            systemctl daemon-reload 2>/dev/null || true
        fi

        if $extracted_archive; then
            echo "  Removing installed files..."
            if [[ -f "$MANIFEST_FILE" ]]; then
                remove_installed_files
            else
                echo "Error: Manifest file not found at $MANIFEST_FILE."
            fi
        elif $created_dir; then
            if [[ -f "$MANIFEST_FILE" ]]; then
                echo "  Removing installed files..."
                remove_installed_files
            else
                echo "  Removing installation directory..."
                rmdir --ignore-fail-on-non-empty "$EVEREST_INSTALL_DIR" 2>/dev/null || true
            fi
        fi

        if $stopped_service; then
            echo "  Restarting systemd service..."
            systemctl start "$EVEREST_SERVICE" 2>/dev/null || true
        fi

        echo "Rollback complete."
        exit 1
    }

    trap rollback INT TERM EXIT

    # Step 1: Stop service if running
    if $service_was_running; then
        echo "Stopping '$EVEREST_SERVICE' service..."
        systemctl stop "$EVEREST_SERVICE"
        stopped_service=true
        echo "  Service stopped."
    fi

    # Step 2: Create installation directory
    echo "Creating installation directory at $EVEREST_INSTALL_DIR..."
    mkdir -p "$EVEREST_INSTALL_DIR"
    created_dir=true
    echo "  Directory created."

    # Step 3: Extract archive
    echo "Extracting firmware archive to $EVEREST_INSTALL_DIR..."
    tar -xmzvf "$ARCHIVE_PATH" -C "$EVEREST_INSTALL_DIR" > "$MANIFEST_FILE"
    extracted_archive=true
    echo "  Archive extracted."

    # Step 4: Backup original service file
    if [[ ! -f "$SERVICE_BACKUP" ]]; then
        echo "Backing up original systemd service to $SERVICE_BACKUP..."
        cp "$SERVICE_FILE" "$SERVICE_BACKUP"
        backed_up_service=true
        echo "  Backup created."
    else
        echo "Backup already exists at $SERVICE_BACKUP, skipping backup."
    fi

    # Step 5: Overwrite service file with template
    echo "Updating systemd service file..."
    sed \
        -e "s|%EverestInstallDir%|$EVEREST_INSTALL_DIR|g" \
        -e "s|%EverestConfigFile%|$EVEREST_CONFIG_FILE|g" \
        "$SERVICE_TEMPLATE" > "$SERVICE_FILE"
    overwrote_service=true
    echo "  Service file updated."

    # Step 6: Reload daemon
    echo "Reloading systemd daemon..."
    systemctl daemon-reload
    echo "  Daemon reloaded."

    trap - INT TERM EXIT

    echo
    echo "Installation successful!"

    # Warn if the specified config file does not exist in the installation
    local config_path="$EVEREST_INSTALL_DIR/everest/config/$EVEREST_CONFIG_FILE"
    if [[ ! -f "$config_path" ]]; then
        echo
        echo "Warning: Config file '$EVEREST_CONFIG_FILE' not found at $config_path."
        echo "The service may fail to start with a missing config file."
        echo "Update EVEREST_CONFIG_FILE in settings.env and run './everest.sh modify' to fix."
    fi

    echo
    echo "Start the service with: systemctl start $EVEREST_SERVICE"
}

# -----------------------------------------------------------------------------
# Uninstall
# -----------------------------------------------------------------------------
do_uninstall() {
    echo "=== Everest Uninstall ==="
    echo

    # Check that a custom installation exists
    if ! is_installed; then
        echo "Error: No existing Everest installation found at $EVEREST_INSTALL_DIR."
        echo "Nothing to uninstall."
        exit 1
    fi

    # Verify the systemd service exists
    if [[ ! -f "$SERVICE_FILE" ]]; then
        echo "Error: Systemd service '$EVEREST_SERVICE' not found at $SERVICE_FILE."
        exit 1
    fi

    # Verify backup exists for restoration
    if [[ ! -f "$SERVICE_BACKUP" ]]; then
        echo "Error: Original service backup not found at $SERVICE_BACKUP."
        echo "Cannot restore original service without a backup."
        exit 1
    fi

    # Check if service is running
    local service_was_running=false
    if is_service_active; then
        service_was_running=true
    fi

    # Inform user of planned actions
    local step=1
    echo "The following actions will be performed:"
    if $service_was_running; then
        echo "  $step. Stop the '$EVEREST_SERVICE' systemd service"
        ((step++))
    fi
    echo "  $step. Remove custom Everest files from: $EVEREST_INSTALL_DIR"
    ((step++))
    echo "  $step. Restore the original systemd service from backup"
    ((step++))
    echo "  $step. Reload the systemd daemon"
    echo

    if ! confirm "Do you wish to proceed?"; then
        echo "Uninstallation cancelled. No changes have been made."
        exit 0
    fi

    echo

    # Step 1: Stop service if running
    if $service_was_running; then
        echo "Stopping '$EVEREST_SERVICE' service..."
        systemctl stop "$EVEREST_SERVICE"
        echo "  Service stopped."
    fi

    # Step 2: Remove custom installation files
    echo "Removing custom Everest files from $EVEREST_INSTALL_DIR..."
    remove_installed_files
    echo "  Installed files removed."

    # Step 3: Restore original service
    echo "Restoring original systemd service..."
    cp "$SERVICE_BACKUP" "$SERVICE_FILE"
    rm -f "$SERVICE_BACKUP"
    echo "  Original service restored."

    # Step 4: Reload daemon
    echo "Reloading systemd daemon..."
    systemctl daemon-reload
    echo "  Daemon reloaded."

    echo
    echo "Uninstallation complete."
    echo "The original '$EVEREST_SERVICE' service has been restored."
}

# -----------------------------------------------------------------------------
# Modify
# -----------------------------------------------------------------------------
do_modify() {
    echo "=== Everest Modify ==="
    echo

    # Check that a custom installation exists
    if ! is_installed; then
        echo "Error: No existing Everest installation found at $EVEREST_INSTALL_DIR."
        echo "Please run './everest.sh install' first."
        exit 1
    fi

    # Verify the systemd service exists
    if [[ ! -f "$SERVICE_FILE" ]]; then
        echo "Error: Systemd service '$EVEREST_SERVICE' not found at $SERVICE_FILE."
        exit 1
    fi

    # Verify the service template exists
    if [[ ! -f "$SERVICE_TEMPLATE" ]]; then
        echo "Error: Service template not found at $SERVICE_TEMPLATE."
        exit 1
    fi

    # Warn if the specified config file does not exist in the installation
    local config_path="$EVEREST_INSTALL_DIR/everest/config/$EVEREST_CONFIG_FILE"
    if [[ ! -f "$config_path" ]]; then
        echo "Warning: Config file '$EVEREST_CONFIG_FILE' not found at $config_path."
        echo "The service may fail to start with a missing config file."
        echo
        if ! confirm "Do you wish to continue anyway?"; then
            echo "Modification cancelled. No changes have been made."
            exit 0
        fi
        echo
    fi

    # Check if service is running
    local service_was_running=false
    if is_service_active; then
        service_was_running=true
    fi

    # Inform user of planned actions
    local step=1
    echo "The following actions will be performed:"
    if $service_was_running; then
        echo "  $step. Stop the '$EVEREST_SERVICE' systemd service"
        ((step++))
    fi
    echo "  $step. Update the systemd service to use config file: $EVEREST_CONFIG_FILE"
    ((step++))
    echo "  $step. Reload the systemd daemon"
    echo

    if ! confirm "Do you wish to proceed?"; then
        echo "Modification cancelled. No changes have been made."
        exit 0
    fi

    echo

    # Step 1: Stop service if running
    if $service_was_running; then
        echo "Stopping '$EVEREST_SERVICE' service..."
        systemctl stop "$EVEREST_SERVICE"
        echo "  Service stopped."
    fi

    # Step 2: Overwrite service file with updated config
    echo "Updating systemd service with new config file..."
    sed \
        -e "s|%EverestInstallDir%|$EVEREST_INSTALL_DIR|g" \
        -e "s|%EverestConfigFile%|$EVEREST_CONFIG_FILE|g" \
        "$SERVICE_TEMPLATE" > "$SERVICE_FILE"
    echo "  Service file updated."

    # Step 3: Reload daemon
    echo "Reloading systemd daemon..."
    systemctl daemon-reload
    echo "  Daemon reloaded."

    echo
    echo "Modification complete."
    echo "The service now uses config file: $EVEREST_CONFIG_FILE"
    echo "Start the service with: systemctl start $EVEREST_SERVICE"
}

# -----------------------------------------------------------------------------
# Help
# -----------------------------------------------------------------------------
show_help() {
    echo "Everest Test System Manager"
    echo
    echo "Usage: ./everest.sh <command>"
    echo
    echo "Commands:"
    echo "  install     Install the custom Everest firmware"
    echo "  uninstall   Uninstall the custom Everest firmware and restore original"
    echo "  modify      Update the Everest service to use a different config file"
    echo "  --help      Show this help message"
    echo
    echo "Configuration:"
    echo "  Edit 'settings.env' to customize the installation before running commands."
    echo
    echo "Examples:"
    echo "  ./everest.sh install     # Install custom Everest firmware"
    echo "  ./everest.sh uninstall   # Remove custom firmware and restore original"
    echo "  ./everest.sh modify      # Change the active config file"
}

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
main() {
    local command="${1:-}"

    case "$command" in
        install)
            load_settings
            do_install
            ;;
        uninstall)
            load_settings
            do_uninstall
            ;;
        modify)
            load_settings
            do_modify
            ;;
        --help|"")
            show_help
            ;;
        *)
            echo "Error: Unknown command '$command'"
            echo
            show_help
            exit 1
            ;;
    esac
}

main "$@"
