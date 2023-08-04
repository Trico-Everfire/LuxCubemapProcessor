cmake_minimum_required(VERSION 3.25)
project(LuxCubemapProcessor)

set(MZ_LZMA OFF CACHE BOOL "" FORCE)
set(MZ_BZIP2 OFF CACHE BOOL "" FORCE)
set(MZ_PKCRYPT OFF CACHE BOOL "" FORCE)
set(MZ_WZAES OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL OFF CACHE BOOL "" FORCE)
set(MZ_LIBBSD OFF CACHE BOOL "" FORCE)
set(MZ_ICONV OFF CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_INSTALL_RPATH $ORIGIN)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(libs/VTFLib)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set_target_properties(
        vtflib PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_subdirectory(libs/minizip-ng)

set_target_properties(
        minizip PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

include_directories(libs/minizip-ng)
include_directories(libs/VTFLib/VTFLib)

set(SRC src/main.cpp src/cubemap_bsp.cpp src/cubemap_bsp.h src/zip_handler.cpp src/zip_handler.h src/crc32.h)


add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} PRIVATE vtflib minizip)