#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

set(CY_EVENT_INCLUDE_FILES
	cy_event.h
	event/cye_looper.h
	event/cye_pipe.h
)

set(CY_EVENT_SOURCE_FILES
	event/cye_looper.cpp
	event/cye_pipe.cpp
)

set(CY_EVENT_INTERNAL_FILES
	event/internal/cye_looper_select.h
	event/internal/cye_looper_epoll.h
	event/internal/cye_looper_select.cpp
	event/internal/cye_looper_epoll.cpp
	event/internal/cye_create_looper.cpp
)
source_group("Source Files\\internal" FILES ${CY_EVENT_INTERNAL_FILES})

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CY_SOURCE_CORE_PATH}
)

add_library(cy-event 
	${CY_EVENT_INCLUDE_FILES} 	
	${CY_EVENT_SOURCE_FILES} 
	${CY_EVENT_INTERNAL_FILES}
)
