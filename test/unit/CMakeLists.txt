#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.8)

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_CRYPT_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${GTEST_INCLUDE_DIRS}
)

link_directories(
    ${GTEST_LIBRARIES_PATH}
)

set(cyt_unit_sources
    cyt_unit_main.cpp
    cyt_unit_lfqueue.cpp
    cyt_unit_crypt.cpp
    cyt_unit_ringbuf.cpp
    cyt_unit_pipe.cpp
    cyt_unit_signal.cpp
    cyt_event_fortest.h
    cyt_unit_event_basic.cpp
    cyt_unit_event_timer.cpp
    cyt_unit_event_socket.cpp
    cyt_uint_system.cpp
    cyt_unit_packet.cpp
)

add_executable(cyt_unit 
    ${cyt_unit_sources}
)

set_property(TARGET cyt_unit PROPERTY FOLDER "test/unit")

if(CY_SYS_WINDOWS)
target_link_libraries(cyt_unit
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${GTEST_BOTH_LIBRARIES}
)

else()

target_link_libraries(cyt_unit
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
	${GTEST_BOTH_LIBRARIES}
)

endif()

GTEST_ADD_TESTS(cyt_unit "" ${cyt_unit_sources})
