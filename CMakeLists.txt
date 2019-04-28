cmake_minimum_required (VERSION 2.6)
project (Exchange)

# GTest
ADD_SUBDIRECTORY (googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

include_directories(${PROJECT_SOURCE_DIR}/include)
add_executable(testsuite testsuite.cpp src/exchange.cpp src/tradingtool.cpp)
target_link_libraries(testsuite gtest gtest_main)
add_test(testsuite testsuite)


