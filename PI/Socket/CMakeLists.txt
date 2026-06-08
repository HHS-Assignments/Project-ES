cmake_minimum_required(VERSION 3.10)
project(SocketCPP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    protocol/JsonMessage.cpp
    protocol/cJSON.c
    transport/SocketConnection.cpp
    transport/HttpServer.cpp
    transport/ThreadPoolWorker.cpp
    dispatch/WmosHandler.cpp
    dispatch/UnknownHandler.cpp
    dispatch/DeviceDispatcher.cpp
    service/PiAService.cpp
    service/PiBService.cpp
    bridge/CanSocketBridge.cpp  # Add this line
)

# Create library
add_library(socketlib ${SOURCES})
target_include_directories(socketlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ============ PROOF OF CONCEPT: RPi_A and RPi_B ============

# RPi_A: TCP Receiver/Dispatcher
add_executable(RPi_A RPi_A.cpp)
target_link_libraries(RPi_A socketlib pthread)
target_include_directories(RPi_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# RPi_B: HTTP Forwarder
add_executable(RPi_B RPi_B.cpp)
target_link_libraries(RPi_B socketlib pthread)
target_include_directories(RPi_B PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ============ Optional: Legacy example and tests ============

# Example executable
add_executable(example example.cpp)
target_link_libraries(example socketlib pthread)

# Optional: enable testing
enable_testing()
add_executable(test_basic tests/test_basic.cpp)
target_link_libraries(test_basic socketlib pthread)
add_test(NAME BasicTest COMMAND test_basic)