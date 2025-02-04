cmake_minimum_required(VERSION 3.15)

project(server)
include(cmake/deps.cmake)
#set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler" FORCE)
#set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ~/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "")
endif()
include(${CMAKE_TOOLCHAIN_FILE})
find_package(websocketpp CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

set(COMMON_COMPILE_DEFINITIONS
        WEBRTC_LINUX
        WEBRTC_POSIX
)

# Common compile options
set(COMMON_COMPILE_OPTIONS
        -Wno-deprecated-declarations
        -Wno-missing-field-initializers
        -Wno-unused-parameter
)

include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB SOURCES "src/*.cpp") 
add_executable(server main.cpp ${SOURCES})
#add_library(server STATIC ${SOURCES})
file(COPY ${CMAKE_SOURCE_DIR}/.env DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/lua DESTINATION ${CMAKE_BINARY_DIR})

target_link_libraries(server PRIVATE pjproject my_webrtc lua5.4 sol2 websocketpp::websocketpp dl ZLIB::ZLIB )
target_compile_definitions(server PRIVATE ${COMMON_COMPILE_DEFINITIONS})
target_compile_options(server PRIVATE ${COMMON_COMPILE_OPTIONS})
# Add debug symbols only in Debug build, and set release options
target_compile_options(server PRIVATE
    $<$<CONFIG:Debug>:-g>
    -Wno-notes
    -Wno-cpp
)

# Linker flags (rdynamic for symbol visibility in Release)
target_link_options(server PRIVATE
    $<$<CONFIG:Release>:-rdynamic>
)

# Optional: Add Link-Time Optimization (LTO) in Release
target_compile_options(server PRIVATE
    $<$<CONFIG:Release>:-flto>
)
target_link_options(server PRIVATE
    $<$<CONFIG:Release>:-flto>
)