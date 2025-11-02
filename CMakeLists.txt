cmake_minimum_required(VERSION 3.15)
project(HSO_Sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hso_sim
    src/workload_generator.cpp
    src/hso_device.cpp
    src/hso_engine.cpp
)

target_include_directories(hso_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)


