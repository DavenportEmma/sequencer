cmake_minimum_required(VERSION 3.27.3)

include(${SDK_BASE}/cmake/common.cmake)

project(sequencer C)

target_sources(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/setup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/menu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sequence.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)