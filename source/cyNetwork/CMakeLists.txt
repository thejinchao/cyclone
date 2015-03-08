#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

set(CY_NETWORK_INCLUDE_FILES
	cy_network.h
	network/cyn_socket.h
	network/cyn_address.h
	network/cyn_tcp_server.h
	network/cyn_connection.h
	network/cyn_work_thread.h
)

set(CY_NETWORK_SOURCE_FILES
	network/cyn_socket.cpp
	network/cyn_address.cpp
	network/cyn_tcp_server.cpp
	network/cyn_connection.cpp
	network/cyn_work_thread.cpp
)

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
)

add_library(cy-network 
	${CY_NETWORK_SOURCE_FILES} 
	${CY_NETWORK_INCLUDE_FILES} 
)
