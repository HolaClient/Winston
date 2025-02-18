#!/bin/bash

mkdir -p build

rm -rf build/*
rm -f webserver

echo "Compiling assembly..."
nasm -f elf64 -DLINUX -O3 src/http/parser.asm -o build/parser.o

echo "Compiling C++..."
g++ -m64 -O3 -march=native -flto \
    -fno-exceptions -fno-rtti \
    -funroll-loops -finline-functions \
    -fomit-frame-pointer -fno-asynchronous-unwind-tables \
    -D__linux__ \
    src/app.cpp \
    src/socket.cpp \
    src/config/config.cpp \
    build/parser.o \
    -o webserver \
    -static-libgcc -static-libstdc++ -no-pie