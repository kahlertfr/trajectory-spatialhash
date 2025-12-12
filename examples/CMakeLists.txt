# Example showing how to use trajectory_spatialhash from another C++ project

add_executable(use_from_other_project
    use_from_other_project.cpp
)

target_link_libraries(use_from_other_project
    PRIVATE trajectory_spatialhash
)
