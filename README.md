# EVSE Test System

The EVSE Test System is a modified [EVerest](https://github.com/EVerest) firmware stack that provides additional modules for testing the interoperability of Electric Vehicles. It allows users to run test scenarios that modify charger behavior, putting vehicles into unexpected or invalid situations to verify that they respond appropriately.

The system supports testing against the **DIN 70121** and **ISO 15118-2** Vehicle-to-Grid (V2G) communication protocols, with both **AC** and **DC** charging modes, and **EIM** (External Identification Mode) and **Plug & Charge** authentication methods.

## Key Features

- 20 conformance test scenarios based on CharIN and ChargeX test specifications.
- Web dashboard for configuring, executing, and reviewing test results.
- REST API for programmatic test control.
- Network traffic capture (PCAP) with TLS key embedding for encrypted session analysis.
- Real-time test status via Server-Sent Events (SSE).
- Software-in-the-Loop (SIL) simulation support for development and testing without hardware.

## Documentation

Detailed documentation is available in the [project wiki](https://github.com/QualityLogic/evse-test-system/wiki):

- **[Home](https://github.com/QualityLogic/evse-test-system/wiki)** - Wiki overview and navigation.
- **[Project Structure](https://github.com/QualityLogic/evse-test-system/wiki/Project-Structure)** - Architecture and module overview.
- **[Development](https://github.com/QualityLogic/evse-test-system/wiki/Project-Development)** - Prerequisites, build, and install instructions.
- **[Running Tests](https://github.com/QualityLogic/evse-test-system/wiki/Running-Tests)** - How to run the test system in simulation and on hardware.
- **[Test Cases](https://github.com/QualityLogic/evse-test-system/wiki/Test-Cases)** - Catalog of all available test scenarios.
- **[Web Dashboard](https://github.com/QualityLogic/evse-test-system/wiki/Web-Dashboard)** - Guide to using the web-based test controller.
- **[REST API](https://github.com/QualityLogic/evse-test-system/wiki/REST-API)** - PyTestServer API endpoint reference.
- **[Configuration](https://github.com/QualityLogic/evse-test-system/wiki/Configuration)** - YAML config file structure and options.
- **[Plug & Charge](https://github.com/QualityLogic/evse-test-system/wiki/Plug-and-Charge)** - Enabling Plug & Charge with OCPP.
- **[Phytec EVCS-Cube Setup](https://github.com/QualityLogic/evse-test-system/wiki/Phytec-EVCS-Cube)** - Hardware setup for the Phytec AC charger.

### Module Documentation

- **[EvseTestManager](https://github.com/QualityLogic/evse-test-system/wiki/EvseTestManager)** - Test orchestration and state machine.
- **[EvseTestV2G](https://github.com/QualityLogic/evse-test-system/wiki/EvseTestV2G)** - V2G protocol test case implementations.

## Quick Start

### Prerequisites

A Linux system with at least 4 GB of RAM for building. See the [Development](https://github.com/QualityLogic/evse-test-system/wiki/Project-Development) wiki page for full prerequisite details and OS-specific package installation.

### Build

```shell
# Install EVerest Dependency Manager
git clone git@github.com:EVerest/everest-dev-environment.git
cd everest-dev-environment/dependency_manager
python3 -m pip install .

# Set up environment
export PATH=$PATH:/home/$(whoami)/.local/bin
export CPM_SOURCE_CACHE=$HOME/.cache/CPM

# Set up EVerest workspace
export EVEREST_WORKSPACE=~/ev-workspace
mkdir -p $EVEREST_WORKSPACE
git clone https://github.com/EVerest/everest-cmake ${EVEREST_WORKSPACE}/everest-cmake
git clone https://github.com/EVerest/everest-framework ${EVEREST_WORKSPACE}/everest-framework
git clone https://github.com/EVerest/everest-utils ${EVEREST_WORKSPACE}/everest-utils

# Install ev-cli
cd $EVEREST_WORKSPACE/everest-utils/ev-dev-tools
python3 -m pip install .

# Clone and build the EVSE Test System
export EVEREST_PROJECT_DIR=~/evse-test-system
git clone git@github.com:QualityLogic/evse-test-system.git $EVEREST_PROJECT_DIR
mkdir -p $EVEREST_PROJECT_DIR/build
cd $EVEREST_PROJECT_DIR/build
CMAKE_PREFIX_PATH=$EVEREST_WORKSPACE cmake --install-prefix $EVEREST_PROJECT_DIR/dist ..
make -j$(nproc) install
```

### Run (Simulation)

```shell
# Start the MQTT broker
docker start mqtt-server

# Run AC simulation (requires root for packet capture)
sudo -E ./build/run-scripts/run-sil-ac-test.sh
```

Once EVerest is running, open `http://localhost:1986` in a browser to access the web dashboard.

## License

This project is licensed under the [Apache License 2.0](LICENSE).
