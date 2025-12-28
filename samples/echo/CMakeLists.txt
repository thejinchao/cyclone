#
#Copyright(C) thecodeway.com
#

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${CY_SOURCE_UTILITY_PATH}
	${CY_UTILITY_PATH}/SimpleOpt
)

add_executable(echo-server echo_server.cpp)
add_executable(echo-client echo_client.cpp)

set_property(TARGET echo-server PROPERTY FOLDER "samples/echo")
set_property(TARGET echo-client PROPERTY FOLDER "samples/echo")

target_link_libraries(echo-server
	cyclone
	${CY_SYSTEM_LIBRARIES}
)

target_link_libraries(echo-client
	cyclone
	${CY_SYSTEM_LIBRARIES}
)
