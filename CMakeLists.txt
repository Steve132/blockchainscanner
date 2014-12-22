cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "-std=c++11")
add_definitions(-DNO_REALPATH)
add_executable(count_transactions
	blockchainscan.cpp
	scantransactions.cpp)
add_executable(firstbitsd firstbitsd.cpp)
add_executable(httptest httptest.cpp)
add_library(mhttpdpp microhttpdpp.cpp mhttpdfiles.cpp)
target_link_libraries(mhttpdpp microhttpd)
target_link_libraries(httptest mhttpdpp)
target_link_libraries(firstbitsd mhttpdpp)
