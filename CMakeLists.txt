cmake_minimum_required(VERSION 3.16)
project(UltraFastAnalysis VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra -fsanitize=address")

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/order_book.cpp
    src/order_matching_engine.cpp
    src/market_data_processor.cpp
    src/tcp_server.cpp
    src/ring_buffer.cpp
    src/order.cpp
    src/market_data.cpp
    src/performance_monitor.cpp
)

# Create library target for the core engine
add_library(order_engine_lib STATIC ${SOURCES})

# Link libraries to the core engine library
target_link_libraries(order_engine_lib 
    PUBLIC ${CMAKE_THREAD_LIBS_INIT}
    PUBLIC Boost::system
    PUBLIC Boost::thread
)

# Create executable that links to the library
add_executable(order_matching_engine src/main.cpp)
target_link_libraries(order_matching_engine 
    PRIVATE order_engine_lib
)

# Python bindings (optional)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
if(BUILD_PYTHON_BINDINGS)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    find_package(pybind11 REQUIRED)
    
    pybind11_add_module(order_engine_python src/python_bindings.cpp)
    
    # Fix: Link to the library target, not the executable
    target_link_libraries(order_engine_python 
        PRIVATE order_engine_lib
    )
endif()

# Testing
enable_testing()
add_subdirectory(tests)

# Installation
install(TARGETS order_matching_engine DESTINATION bin)
