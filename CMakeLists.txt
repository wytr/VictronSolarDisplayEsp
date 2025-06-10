cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(VictronSolarDisplayEsp)

spiffs_create_partition_image(spiffs 
    "${CMAKE_SOURCE_DIR}/files" 
    FLASH_IN_PROJECT
)