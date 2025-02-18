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
    COMMAND ${NASM_EXECUTABLE} -f ${NASM_FORMAT} -o ${CMAKE_BINARY_DIR}/server.o ${CMAKE_SOURCE_DIR}/src/server.asm -DPIC
    DEPENDS ${CMAKE_SOURCE_DIR}/src/server.asm
    COMMENT "Building ASM object"
)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

if(UNIX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
endif()

add_executable(hcw
    src/main.cpp
    ${CMAKE_BINARY_DIR}/server.o
)

if(UNIX)
    target_link_libraries(hcw PRIVATE pthread)
endif()