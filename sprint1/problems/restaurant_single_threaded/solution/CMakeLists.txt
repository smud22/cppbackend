cmake_minimum_required(VERSION 3.11)

project(restaurant CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(restaurant src/main.cpp)
target_link_libraries(restaurant PRIVATE Threads::Threads)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_compile_options(restaurant PRIVATE -fexperimental-library)
    target_link_options(restaurant PRIVATE -fexperimental-library)
endif()