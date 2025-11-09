# Oracle sim binary
add_executable(oracle-sim main.cpp)

target_link_libraries(oracle-sim PRIVATE
    sim_core
    Boost::system
    Boost::thread
    spdlog::spdlog
    yaml-cpp
    nlohmann_json::nlohmann_json
)

target_compile_features(oracle-sim PRIVATE cxx_std_20)

install(TARGETS oracle-sim
    RUNTIME DESTINATION bin
)