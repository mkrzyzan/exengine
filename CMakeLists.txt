cmake_minimum_required (VERSION 2.6)
project (Exchange)

# GTest
ADD_SUBDIRECTORY (googletest)
enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(beczka beczka.cpp)
target_link_libraries(beczka gtest gtest_main)
add_test(beczka beczka)


