#!/bin/bash
set -euo pipefail

# =============================================================================
# Everest Test System - Installer Upload
# =============================================================================

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd -P)"
SETTINGS_FILE="$SCRIPT_DIR/settings.env"

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
    [[ -z "${REMOTE_HOST:-}" ]] && missing+=("REMOTE_HOST")
    [[ -z "${REMOTE_USER:-}" ]] && missing+=("REMOTE_USER")
    [[ -z "${REMOTE_INSTALLER_PATH:-}" ]] && missing+=("REMOTE_INSTALLER_PATH")
    [[ -z "${LOCAL_INSTALLER_DIR:-}" ]] && missing+=("LOCAL_INSTALLER_DIR")

    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "Error: The following required settings are missing in $SETTINGS_FILE:"
        printf '  - %s\n' "${missing[@]}"
        exit 1
    fi

    INSTALLER_PATH="$SCRIPT_DIR/$LOCAL_INSTALLER_DIR"
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

is_host_reachable() {
    local os_name
    os_name="$(uname -s)"

    if [[ "$os_name" == "Darwin" ]]; then
        # macOS: -c 1 sends one packet, -i 1 sets 1 second timeout
        ping -c 1 -i 1 "$REMOTE_HOST" > /dev/null 2>&1
    else
        # Windows/Linux: -n 1 sends one packet, -w 1000 sets 1 second timeout (ms)
        ping -n 1 -w 1000 "$REMOTE_HOST" > /dev/null 2>&1
    fi
}

# -----------------------------------------------------------------------------
# Upload
# -----------------------------------------------------------------------------
do_upload() {
    echo "=== Everest Upload ==="
    echo

    # Verify the local installer directory exists
    if [[ ! -d "$INSTALLER_PATH" ]]; then
        echo "Error: Local installer directory not found at $INSTALLER_PATH."
        exit 1
    fi

    # Verify the remote host is reachable
    echo "Checking connectivity to $REMOTE_HOST..."
    if ! is_host_reachable; then
        echo "Error: The remote system '$REMOTE_HOST' is unreachable or unresponsive."
        exit 1
    fi
    echo "  Host is reachable."
    echo

    # Determine approximate transfer size
    local installer_size
    installer_size=$(du -sh "$INSTALLER_PATH" | awk '{print $1}')

    # Inform user and ask for confirmation
    echo "The following action will be performed:"
    echo "  1. Copy installer files (~${installer_size}B) to $REMOTE_USER@$REMOTE_HOST:$REMOTE_INSTALLER_PATH"
    echo

    if ! confirm "Do you wish to proceed?"; then
        echo "Upload cancelled. No files have been copied."
        exit 0
    fi

    echo

    # Copy files to remote host
    echo "Copying installer files to $REMOTE_USER@$REMOTE_HOST:$REMOTE_INSTALLER_PATH..."
    scp -r "$INSTALLER_PATH" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_INSTALLER_PATH"
    echo "  Files copied."

    echo
    echo "Upload complete!"
}

# -----------------------------------------------------------------------------
# Help
# -----------------------------------------------------------------------------
show_help() {
    echo "Everest Test System - Installer Upload"
    echo
    echo "Copies the installer directory to the remote system via SCP"
    echo
    echo "Usage: ./upload.sh <command>"
    echo
    echo "Commands:"
    echo "  --help      Show this help message"
    echo
    echo "Configuration:"
    echo "  Edit 'settings.env' to configure the remote host and paths."
}

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
main() {
    local command="${1:-}"

    case "$command" in
        "")
            load_settings
            do_upload
            ;;
        --help)
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
