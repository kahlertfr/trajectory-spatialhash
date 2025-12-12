include(FetchContent)

# Fetch Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Test executable
add_executable(test_spatial_hash
    test_spatial_hash.cpp
)

target_link_libraries(test_spatial_hash
    PRIVATE
        trajectory_spatialhash
        Catch2::Catch2WithMain
)

# Discover tests
include(CTest)
include(Catch)
catch_discover_tests(test_spatial_hash)
