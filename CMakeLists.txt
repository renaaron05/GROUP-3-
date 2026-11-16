cmake_minimum_required(VERSION 3.16)

project(GROUP_03_CAR_DEALERSHIP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets PrintSupport)

qt_standard_project_setup()

qt_add_executable(GROUP_03_CAR_DEALERSHIP
    main.cpp
)

target_link_libraries(GROUP_03_CAR_DEALERSHIP
    PRIVATE Qt6::Widgets Qt6::PrintSupport
)

set_target_properties(GROUP_03_CAR_DEALERSHIP PROPERTIES
    WIN32_EXECUTABLE TRUE
)