#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.8)

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
)

add_executable(socks5
    s5_main.cpp
    s5_protocol.h
    s5_protocol.cpp
)

set_property(TARGET socks5 PROPERTY FOLDER "samples/socks5")

if(CY_SYS_WINDOWS)
target_link_libraries(socks5
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

else()

target_link_libraries(socks5
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

endif()
