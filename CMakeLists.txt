cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "-std=c++11")
add_executable(count_transactions
	blockchainscan.cpp
	scantransactions.cpp)
add_executable(firstbitsd firstbitsd.cpp)
