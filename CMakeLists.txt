cmake_minimum_required (VERSION 2.6)
project (Exchange)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=sandybridge")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3" )

# GTest
ADD_SUBDIRECTORY (googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

include_directories(${PROJECT_SOURCE_DIR}/include)
add_executable(testsuite testsuite.cpp src/exchange.cpp src/tradingtool.cpp src/threadable.cpp)
target_link_libraries(testsuite gtest gtest_main)
add_test(testsuite testsuite)


