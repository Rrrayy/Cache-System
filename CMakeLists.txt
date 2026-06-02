
cmake_minimum_required(VERSION 3.10)

project(
    CaCheSystem
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra)
endif()

set(INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/RArcCache
)

add_executable(
    test_cache Rtest.cpp
)

target_include_directories(
    test_cache
    PRIVATE
    .RArcCache
)

install(
    TARGETS test_cache
    RUNTIME DESTINATION bin
)
