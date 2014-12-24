cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "-std=c++11")
#add_definitions(-DNO_REALPATH)
include_directories(lib)
add_executable(print_addresses
	blockchainscan.cpp
	print_addresses.cpp
	
	)
	
add_executable(firstbitsd firstbitsd.cpp firstbits.cpp blockchainscan.cpp)

add_executable(httptest httptest.cpp)

add_library(mhttpdpp microhttpdpp.cpp mhttpdfiles.cpp)
add_library(bitcoinaddress lib/BitcoinAddress.cpp lib/BitcoinAddress.h)
target_link_libraries(print_addresses bitcoinaddress)
target_link_libraries(mhttpdpp microhttpd)
target_link_libraries(httptest mhttpdpp)
target_link_libraries(firstbitsd mhttpdpp bitcoinaddress)
