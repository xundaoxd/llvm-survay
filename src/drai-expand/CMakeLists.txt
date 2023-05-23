include(AddLLVM)
include(AddClang)

set(LLVM_LINK_COMPONENTS
    Support
)
add_clang_executable(drai-expand drai-expand.cpp)
target_include_directories(drai-expand
    PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS})
target_link_libraries(drai-expand
    PRIVATE
    clangFrontend
    clangTooling
    fmt::fmt
)

