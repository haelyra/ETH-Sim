# Core lib
add_library(sim_core INTERFACE)

target_include_directories(sim_core INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(sim_core INTERFACE
    Boost::system
    Boost::thread
    spdlog::spdlog
    /opt/homebrew/lib/libyaml-cpp.dylib
    nlohmann_json::nlohmann_json
)

target_include_directories(sim_core INTERFACE /opt/homebrew/include)
target_link_directories(sim_core INTERFACE /opt/homebrew/lib)

target_compile_features(sim_core INTERFACE cxx_std_20)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/sim_core
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)