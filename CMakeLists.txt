cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "-std=c++11")
#add_definitions(-DNO_REALPATH)
include_directories(lib)
add_executable(print_addresses
	print_addresses.cpp
	)
add_library(blockchainscan blockchainscan.h blockchainscan.cpp)
add_library(bitcoinaddress lib/BitcoinAddress.cpp lib/BitcoinAddress.h)
target_link_libraries(print_addresses bitcoinaddress blockchainscan)
