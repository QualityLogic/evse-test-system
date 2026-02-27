cmake_minimum_required(VERSION 3.16)

project(everest-test-system
    VERSION 0.1
    DESCRIPTION "An open test system for e-mobility charging stations"
    LANGUAGES CXX C
)

find_package(everest-cmake 0.5 REQUIRED
    COMPONENTS bundling
    PATHS ../everest-cmake
)

# options
option(BUILD_TESTING "Run unit tests" OFF)
option(CMAKE_RUN_CLANG_TIDY "Run clang-tidy" OFF)

# dependencies
if (NOT DISABLE_EDM)
    evc_setup_edm()
else()
    find_package(everest-core)
    # InfyPowerACDC uses pal-sigslot
    find_package(PalSigslot REQUIRED)
endif()

ev_add_project()

# config
add_subdirectory(config)

# logging
# configure_file(logging.ini "${CMAKE_CURRENT_BINARY_DIR}/logging.ini" COPYONLY)
install(
    FILES "logging.ini"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/everest"
    RENAME "default_logging.cfg"
)

# configure clang-tidy if requested
if (CMAKE_RUN_CLANG_TIDY)
    message("Enabling clang-tidy")
    set(CMAKE_CXX_CLANG_TIDY clang-tidy)
endif()

# testing
if (BUILD_TESTING)
    include(CTest)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    evc_include(CodeCoverage)
    append_coverage_compiler_flags()
    add_subdirectory(tests)
    setup_target_for_coverage_gcovr_html(
        NAME gcovr_coverage
        EXECUTABLE test_config
        DEPENDENCIES test_config everest
    )
    setup_target_for_coverage_lcov(
        NAME lcov_coverage
        EXECUTABLE test_config
        DEPENDENCIES test_config everest
    )
endif()
