#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

set(CY_CORE_INCLUDE_FILES
	cy_core.h
	core/cyc_logger.h
	core/cyc_socket_api.h
	core/cyc_thread_api.h
	core/cyc_ring_buf.h
)

set(CY_CORE_SOURCE_FILES 
	core/cyc_logger.cpp
	core/cyc_socket_api.cpp
	core/cyc_thread_api.cpp
	core/cyc_ring_buf.cpp
)

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(cy-core 
	${CY_CORE_SOURCE_FILES} 
	${CY_CORE_INCLUDE_FILES} 
)

