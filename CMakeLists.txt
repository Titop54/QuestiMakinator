cmake_minimum_required(VERSION 3.20)

# Establecer estándar de C++
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR MINGW)
    message(STATUS "Configuración para Windows (Mingw) detectada")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++ -mwindows")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static -static-libgcc")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
else()
    message(STATUS "Configuración para Linux detectada")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")
endif()

project(QuestiMakinator VERSION 1.0.0 LANGUAGES CXX)

# Usar VCPKG_INSTALLED_DIR si está definido
if(DEFINED VCPKG_INSTALLED_DIR)
    message(STATUS "Usando VCPKG_INSTALLED_DIR: ${VCPKG_INSTALLED_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}")
endif()

find_package(SFML COMPONENTS Network Graphics Window Audio System CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)

# Archivos fuente (solo .cpp)
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Ejecutable principal
add_executable(${PROJECT_NAME} ${SOURCES})

# Incluir directorios
target_include_directories(${PROJECT_NAME} PRIVATE 
    "src"
    "include"
)

# Enlazar librerías
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Network
    SFML::Graphics
    SFML::Window
    SFML::Audio
    SFML::System
    ImGui-SFML::ImGui-SFML
)

# Configuración de instalación
install(TARGETS ${PROJECT_NAME} DESTINATION .)