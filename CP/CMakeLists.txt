cmake_minimum_required(VERSION 3.10)
project(CourseWork)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(lib binary_allocator.cpp)
add_library(lib2 buddy_allocator.cpp)

add_executable(main main.cpp)
target_include_directories(lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(lib2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(main PRIVATE lib lib2)
