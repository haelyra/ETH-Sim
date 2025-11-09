# DEX Sim binary
add_executable(dex-sim main.cpp)

target_link_libraries(dex-sim PRIVATE
    sim_core
    Boost::system
    Boost::thread
    spdlog::spdlog
    yaml-cpp
    nlohmann_json::nlohmann_json
)

target_compile_features(dex-sim PRIVATE cxx_std_20)

install(TARGETS dex-sim
    RUNTIME DESTINATION bin
)