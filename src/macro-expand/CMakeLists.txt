include(AddLLVM)
include(AddClang)

set(LLVM_LINK_COMPONENTS
    Support
)
add_clang_executable(macro-expand macro-expand.cpp)
target_include_directories(macro-expand
    PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS})
target_link_libraries(macro-expand
    PRIVATE
    clangFrontend
    clangTooling
)

