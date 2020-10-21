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

add_executable(udp-echo-server 
	udp_echo_server.cpp)

add_executable(udp-echo-client 
	udp_echo_client.cpp)
	
set_property(TARGET udp-echo-server PROPERTY FOLDER "samples/udp")
set_property(TARGET udp-echo-client PROPERTY FOLDER "samples/udp")

if(CY_SYS_WINDOWS)
target_link_libraries(udp-echo-server
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

target_link_libraries(udp-echo-client
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

else()

target_link_libraries(udp-echo-server
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

target_link_libraries(udp-echo-client
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

endif()
