cmake_minimum_required(VERSION 3.20)

# Basic project configuration
project(QuestiMakinator VERSION 1.0.0 LANGUAGES CXX)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using CCache")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
else()
    message(WARNING "No CCache detected - skipping")
endif()

# C++ standard configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Toolchain configuration (vcpkg)
if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        message(STATUS "Using enviroment vcpkg root variable")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    else()
        message(STATUS "Using cwd path to the toolchain")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()

    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE} CACHE STRING "Vcpkg toolchain file")
endif()

# Vcpkg installed directory configuration
if(DEFINED VCPKG_INSTALLED_DIR)
    message(STATUS "Using VCPKG_INSTALLED_DIR: ${VCPKG_INSTALLED_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}")
endif()

# Platform-specific configuration
option(DYNAMIC_BUILD "Enable dynamic linking" OFF)
string(TOUPPER "${CMAKE_BUILD_TYPE}" U_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_program(MOLD_PATH mold)
find_program(LLD_PATH lld)

if(MOLD_PATH AND NOT (WIN32 OR MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    message(STATUS "Using mold as linker")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold")
elseif(LLD_PATH)
    message(STATUS "Using lld as linker")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
else()
    message(WARNING "Using default linker, you should use lld or mold")
endif()

if(U_BUILD_TYPE MATCHES "RELEASE|MINSIZEREL")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    
    #--time
    if(USE_TIME_TRACE)
        message(STATUS "Generating .json that you can use on chrome://tracing")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
    endif()
endif()

if(WIN32 OR MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Windows (Mingw) configuration detected")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
    
    if(NOT DYNAMIC_BUILD)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif()
else()
    message(STATUS "Linux configuration detected")
    
    if(NOT U_BUILD_TYPE MATCHES "RELEASE|MINSIZEREL")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gsplit-dwarf -fsanitize=address -fno-omit-frame-pointer")
    endif()
endif()

# Dependency search
find_package(SFML COMPONENTS Network Graphics Audio System CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Backward CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# Source files configuration (this can be done with all .cpp)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Executable creation
add_executable(${PROJECT_NAME} 
    ${SOURCES}
    resources.rc
)

# Include directories configuration
target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    include
)

target_precompile_headers(${PROJECT_NAME} PRIVATE 
    <iostream>
    <string>
    <vector>
    <unordered_map>
    <map>
    <functional>

    <SFML/Graphics.hpp>
    <SFML/System.hpp>
    <SFML/Network.hpp>

    <nlohmann/json.hpp>
    <imgui.h>
    <imgui-SFML.h>
)

# Library linking
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Network
    SFML::Graphics
    SFML::Audio
    SFML::System
    ImGui-SFML::ImGui-SFML
    Backward::Backward
    nlohmann_json::nlohmann_json
    WebP::webp
    WebP::webpdecoder
    WebP::webpdemux
    WebP::libwebpmux
    tinyobjloader::tinyobjloader
    glfw
)

# Installation configuration
install(TARGETS ${PROJECT_NAME} DESTINATION .)