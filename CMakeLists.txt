cmake_minimum_required(VERSION 3.20)
project(eth-sim VERSION 0.2.0 LANGUAGES CXX)

# C++20 required for coroutines
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2)
    endif()
endif()

# Find dependencies
# Boost: Set up paths for Homebrew on macOS
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Add Homebrew paths for macOS
if(APPLE)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" "/opt/homebrew" "/usr/local")
    # Explicitly set Boost root if installed via Homebrew
    if(EXISTS "/opt/homebrew/opt/boost")
        set(Boost_ROOT "/opt/homebrew/opt/boost")
    elseif(EXISTS "/usr/local/opt/boost")
        set(Boost_ROOT "/usr/local/opt/boost")
    endif()
endif()

# Suppress CMP0167 warning about FindBoost being deprecated
cmake_policy(SET CMP0167 NEW)

# Find Boost - in Boost 1.89+, system is header-only, so we only need thread
# But we'll try to find system anyway for compatibility
find_package(Boost 1.75 REQUIRED COMPONENTS thread)

# Create Boost::system target if it doesn't exist (header-only in newer Boost)
if(NOT TARGET Boost::system)
    add_library(Boost::system INTERFACE IMPORTED)
    set_target_properties(Boost::system PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
    )
endif()

find_package(spdlog REQUIRED)
# find_package(yaml-cpp REQUIRED)  # Using hardcoded path instead
find_package(nlohmann_json 3.9 REQUIRED)

# Optional: Prometheus C++ client for metrics
# find_package(prometheus-cpp QUIET)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Subdirectories
add_subdirectory(src/core)
add_subdirectory(src/dex_sim)
add_subdirectory(src/oracle_sim)

# Optional: tests
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS dex-sim oracle-sim
    RUNTIME DESTINATION bin
)

# Install configs and static files
install(DIRECTORY configs/ DESTINATION share/eth-sim/configs)
install(DIRECTORY static/ DESTINATION share/eth-sim/static)

# Print configuration summary
message(STATUS "")
message(STATUS "ETH-sim Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Boost version: ${Boost_VERSION}")
message(STATUS "  spdlog version: ${spdlog_VERSION}")
message(STATUS "  yaml-cpp: /opt/homebrew/lib/libyaml-cpp.dylib (hardcoded)")
message(STATUS "  nlohmann_json version: ${nlohmann_json_VERSION}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "")
