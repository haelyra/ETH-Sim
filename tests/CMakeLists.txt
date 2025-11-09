# Unit tests using Google Test

# Find Google Test
find_package(GTest REQUIRED)

# Core library tests
add_executable(test_core test_core.cpp)

target_link_libraries(test_core PRIVATE
    sim_core
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

target_compile_features(test_core PRIVATE cxx_std_20)

# Add test to CTest
add_test(NAME CoreTests COMMAND test_core)

# Enable testing
enable_testing()
