#!/usr/bin/env bash
#
# Recompile and install the project for immediate use.

DEFAULT_CPM_SOURCE_CACHE="/home/$(whoami)/.cache/CPM"
DEFAULT_EVEREST_WORKSPACE_1="/home/$(whoami)/ev-workspace"
DEFAULT_EVEREST_WORKSPACE_2="/home/$(whoami)/everest-workspace"

# get_cpu_count()
# Determines an ideal number of CPU cores to use for multi-threaded tasks.
# The ideal count is determined by max(1, max(cpu_count * 0.8, cpu_count - 2)).
function get_cpu_count() {
  local cpu_count

  # Fetch the number of available CPU cores
  if ! cpu_count=$(nproc --all); then
    cpu_count=1 # Defaults to 1 CPU core on failure
  else
    local count_a="$(( cpu_count - 2 ))"          # count_a = cpu_count - 2
    local count_b="$(( (cpu_count * 80) / 100 ))" # count_b = floor(cpu_count * 0.8)

    cpu_count="$(( count_a > count_b ? count_a : count_b ))" # max(count_a, count_b)
    cpu_count="$(( cpu_count > 1 ? cpu_count : 1 ))"         # max(1, cpu_count)
  fi

  echo $cpu_count
}

# Check if the CPM_SOURCE_CACHE environment variable is unset
if [ "${CPM_SOURCE_CACHE+set}" != set ]; then
  # Check if the default CPM_SOURCE_CACHE directory exists
  if [ -d "$DEFAULT_CPM_SOURCE_CACHE" ]; then
    # Use the existing CPM_SOURCE_CACHE directory
    export CPM_SOURCE_CACHE="$DEFAULT_CPM_SOURCE_CACHE"
  else
    # Check if the user wants to continue without CPM_SOURCE_CACHE
    read -p "CPM_SOURCE_CACHE is unset. Continue? (y/n) " -r choice
    case "$choice" in
      [yY][eE][sS]|[yY])
        ;;
      *)
        echo "Operation cancelled."
        exit 1 # Exit the script if the user does not confirm
        ;;
    esac
  fi
fi

# Check if the EVEREST_WORKSPACE environment variable is unset
if [ "${EVEREST_WORKSPACE+set}" != set ]; then
  # Check if the default EVEREST_WORKSPACE directory exists
  if [ -d "$DEFAULT_EVEREST_WORKSPACE_1" ]; then
    # Use the existing EVEREST_WORKSPACE directory
    export EVEREST_WORKSPACE="$DEFAULT_EVEREST_WORKSPACE_1"
  elif [ -d "$DEFAULT_EVEREST_WORKSPACE_2" ]; then
    # Use the existing EVEREST_WORKSPACE directory
    export EVEREST_WORKSPACE="$DEFAULT_EVEREST_WORKSPACE_2"
  else
    # Exit the script if the workspace directory is missing
    echo "Cannot find the Everest workspace. Specify with \"export EVEREST_WORKSPACE=\$HOME/path/to/workspace\""
    exit 1
  fi
fi

# Get the directory of the current script
SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"

# Change to the script's directory and then to its parent
cd "$SCRIPT_DIR" || exit 1 # Change to script's directory, exit if failed
cd .. || exit 1            # Change to parent directory, exit if failed

export EVEREST_PROJECT_DIR

# Fetch the current working directory
if ! EVEREST_PROJECT_DIR="$(pwd)"; then
  echo "Failed to fetch current working directory"
  exit 1
fi

# Ensure the build directory exists
mkdir -p "$EVEREST_PROJECT_DIR/build"

# Change to the build directory
if ! cd "$EVEREST_PROJECT_DIR/build"; then
  echo "Failed to change to the build directory ($EVEREST_PROJECT_DIR/build)"
  exit 1
fi

# Set the CMAKE_PREFIX_PATH
export CMAKE_PREFIX_PATH="$EVEREST_WORKSPACE"

# Recompile and install the project
cpu_count=$(get_cpu_count)

echo "Building with $cpu_count cpu cores"

cmake --install-prefix "$EVEREST_PROJECT_DIR/dist" ..
make -j"$cpu_count"
make -j"$cpu_count" install
