cmake_minimum_required(VERSION 3.25)
project(LuxCubemapProcessor)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(libs/VTFLib)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

set_target_properties(
        vtflib PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_subdirectory(libs/minizip-ng)
include_directories(libs/minizip-ng)
include_directories(libs/VTFLib/VTFLib)

set(SRC src/main.cpp src/cubemap_bsp.cpp src/cubemap_bsp.h src/zip_handler.cpp src/zip_handler.h src/crc32.h)


add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} PRIVATE vtflib minizip)