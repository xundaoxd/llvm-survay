cmake_minimum_required(VERSION 3.17)
project(llvm-survay)

find_package(LLVM REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})

find_package(Clang REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${CLANG_CMAKE_DIR})

include(FetchContent)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.0.0
)
FetchContent_MakeAvailable(fmt)

add_subdirectory(src)

