cmake_minimum_required(VERSION 3.20.0)

project(Tray)

add_library(ICUESDK SHARED IMPORTED)

add_library(core
    src/main.cpp
)

set_target_properties(ICUESDK PROPERTIES 
IMPORTED_LOCATION "third_partry/iCUESDK.x64_2019.dll"
IMPORTED_IMPLIB "third_partry/iCUESDK.x64_2019.lib")

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME} PRIVATE ICUESDK)