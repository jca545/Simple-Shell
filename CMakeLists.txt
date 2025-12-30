cmake_minimum_required(VERSION 3.14)

project(ShellAssignment VERSION 1.0 DESCRIPTION "A8 - Shell." LANGUAGES C CXX)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Debug
set(SANITIZER_FLAGS "-fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANITIZER_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${SANITIZER_FLAGS}")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} ${SANITIZER_FLAGS}")

file(GLOB SOURCE "src/*.c")
add_executable(shell ${SOURCE})

include(FetchContent)
FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP false
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(shell_test gtest/shell_test.cpp)
target_link_libraries(shell_test GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(shell_test)
