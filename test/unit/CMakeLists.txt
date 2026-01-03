#
#Copyright(C) thecodeway.com
#

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_CRYPT_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${CY_SOURCE_UTILITY_PATH}
)

set(cyt_unit_sources
	catch2/catch.hpp
	cyt_unit_utils.h
	cyt_unit_main.cpp
	cyt_unit_lfqueue.cpp
	cyt_unit_crypt.cpp
	cyt_unit_ring_buf.cpp
	cyt_unit_pipe.cpp
	cyt_event_fortest.h
	cyt_unit_event_basic.cpp
	cyt_unit_event_timer.cpp
	cyt_unit_event_socket.cpp
	cyt_unit_system.cpp
	cyt_unit_system_signal.cpp
	cyt_unit_packet.cpp
	cyt_unit_statistics.cpp
	cyt_unit_ring_queue.cpp
)

add_executable(cyt_unit 
	${cyt_unit_sources}
)

set_property(TARGET cyt_unit PROPERTY FOLDER "test/unit")

if(CY_SYS_ANDROID)
	list(APPEND CY_SYSTEM_LIBRARIES log)
endif()

target_link_libraries(cyt_unit
	cyclone
	${CY_SYSTEM_LIBRARIES}
)
