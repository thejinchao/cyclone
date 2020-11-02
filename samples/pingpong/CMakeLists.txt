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
	${CY_UTILITY_PATH}/SimpleOpt
)

add_executable(pingpong-server
	pingpong_server.cpp
	pingpong_common.h)

add_executable(pingpong-client
	pingpong_client.cpp
	pingpong_common.h)

set_property(TARGET pingpong-server PROPERTY FOLDER "samples/pingpong")
set_property(TARGET pingpong-client PROPERTY FOLDER "samples/pingpong")

if(CY_SYS_WINDOWS)
target_link_libraries(pingpong-server
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

target_link_libraries(pingpong-client
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)
else()

target_link_libraries(pingpong-server
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

target_link_libraries(pingpong-client
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)
endif()
