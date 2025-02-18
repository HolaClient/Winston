cmake_minimum_required(VERSION 3.10)
project(hcw)

find_program(NASM_EXECUTABLE NAMES nasm)
if(NOT NASM_EXECUTABLE)
    message(FATAL_ERROR "NASM is required but not found!")
endif()

if(WIN32)
    set(NASM_FORMAT "win64")
else()
    set(NASM_FORMAT "elf64")
endif()

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/server.o
    COMMAND ${NASM_EXECUTABLE} -f ${NASM_FORMAT} -o ${CMAKE_BINARY_DIR}/server.o ${CMAKE_SOURCE_DIR}/src/server.asm
    DEPENDS ${CMAKE_SOURCE_DIR}/src/server.asm
    COMMENT "Building ASM object"
)

add_executable(hcw
    src/main.cpp
    ${CMAKE_BINARY_DIR}/server.o
)

if(UNIX)
    target_link_libraries(hcw PRIVATE pthread)
endif()