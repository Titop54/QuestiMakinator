cmake_minimum_required(VERSION 3.20)

# Basic project configuration
project(QuestiMakinator VERSION 1.0.0 LANGUAGES CXX)

# C++ standard configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Toolchain configuration (vcpkg)
if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE} CACHE STRING "Vcpkg toolchain file")
endif()

# Platform-specific configuration
if(WIN32 OR MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Windows (Mingw) configuration detected")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
else()
    message(STATUS "Linux configuration detected")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")
endif()

# Vcpkg installed directory configuration
if(DEFINED VCPKG_INSTALLED_DIR)
    message(STATUS "Using VCPKG_INSTALLED_DIR: ${VCPKG_INSTALLED_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}")
endif()

# Dependency search
find_package(SFML COMPONENTS Network Graphics Window Audio System CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)

# Source files configuration (this can be done with all .cpp)
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Executable creation
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories configuration
target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    include
)

# Library linking
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Network
    SFML::Graphics
    SFML::Window
    SFML::Audio
    SFML::System
    ImGui-SFML::ImGui-SFML
)

# Installation configuration
install(TARGETS ${PROJECT_NAME} DESTINATION .)