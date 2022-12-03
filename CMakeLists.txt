cmake_minimum_required(VERSION 3.0)

project(jkport)

if(NOT DEFINED ${CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_PRX ON)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.c)
add_executable(${PROJECT_NAME} ${sources})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    pspgu
    pspge
    pspdisplay
    pspctrl
    debug pspdebug
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE ${PROJECT_NAME}
)
