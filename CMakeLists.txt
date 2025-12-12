cmake_minimum_required(VERSION 3.16)
project(trajectory_spatialhash VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_CLI "Build CLI tool" ON)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Library target
add_library(trajectory_spatialhash
    src/spatial_hash.cpp
    src/ue_wrapper.cpp
)

# Set library properties
target_include_directories(trajectory_spatialhash
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compile definitions
if(BUILD_SHARED_LIBS)
    target_compile_definitions(trajectory_spatialhash
        PRIVATE TSH_BUILDING_LIBRARY
        INTERFACE TSH_USING_SHARED
    )
else()
    target_compile_definitions(trajectory_spatialhash
        PUBLIC TSH_STATIC_LIBRARY
    )
endif()

# Platform-specific settings
if(MSVC)
    target_compile_options(trajectory_spatialhash PRIVATE /W4)
else()
    target_compile_options(trajectory_spatialhash PRIVATE -Wall -Wextra -pedantic)
endif()

# CLI executable
if(BUILD_CLI)
    add_executable(trajectory_spatialhash_cli
        src/cli_main.cpp
    )
    target_link_libraries(trajectory_spatialhash_cli PRIVATE trajectory_spatialhash)
    
    # Set output name
    set_target_properties(trajectory_spatialhash_cli PROPERTIES OUTPUT_NAME tsh)
endif()

# Testing
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install rules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library
install(TARGETS trajectory_spatialhash
    EXPORT trajectory_spatialhash-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY include/trajectory_spatialhash
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install CLI if built
if(BUILD_CLI)
    install(TARGETS trajectory_spatialhash_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Export targets
install(EXPORT trajectory_spatialhash-targets
    FILE trajectory_spatialhash-targets.cmake
    NAMESPACE trajectory_spatialhash::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trajectory_spatialhash
)

# Create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/trajectory_spatialhash-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/trajectory_spatialhash-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trajectory_spatialhash
)

# Create version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/trajectory_spatialhash-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/trajectory_spatialhash-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/trajectory_spatialhash-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trajectory_spatialhash
)
