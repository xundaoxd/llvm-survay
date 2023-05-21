#!/bin/bash

export LLVM_ROOT=$PWD/thirdparty/llvm-root
export Clang_ROOT=$PWD/thirdparty/llvm-root

cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cmake --build build

