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

add_executable(ft-server 
	ft_server.cpp 
	ft_common.h)

add_executable(ft-client 
	ft_client.cpp 
	ft_common.h)

set_property(TARGET ft-server PROPERTY FOLDER "samples/filetransfer")
set_property(TARGET ft-client PROPERTY FOLDER "samples/filetransfer")

target_link_libraries(ft-server
	cyclone
	${CY_SYSTEM_LIBRARIES}
)

target_link_libraries(ft-client
	cyclone
	${CY_SYSTEM_LIBRARIES}
)
