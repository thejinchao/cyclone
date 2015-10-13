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

add_executable(echo-server echo_server.cpp)
add_executable(echo-client echo_client.cpp)

set_property(TARGET echo-server PROPERTY FOLDER "test")
set_property(TARGET echo-client PROPERTY FOLDER "test")

if(CY_SYS_WINDOWS)
target_link_libraries(echo-server
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

target_link_libraries(echo-client
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

else()

target_link_libraries(echo-server
	cyclone
	${JEMALLOC_LIBRARIES}
	pthread
)

target_link_libraries(echo-client
	cyclone
	${JEMALLOC_LIBRARIES}
	pthread
)
endif()
