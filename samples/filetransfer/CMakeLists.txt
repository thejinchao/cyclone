#
#Copyright(C) thecodeway.com
#

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_CRYPT_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${CY_UTILITY_PATH}/SimpleOpt
)

add_executable(ft-server 
	ft_server.cpp 
	ft_common.h)

add_executable(ft-client 
	ft_client.cpp 
	ft_common.h)

set_property(TARGET ft-server PROPERTY FOLDER "samples/filetransfer")
set_property(TARGET ft-client PROPERTY FOLDER "samples/filetransfer")

if(CY_SYS_WINDOWS)
target_link_libraries(ft-server
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

target_link_libraries(ft-client
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

else()

target_link_libraries(ft-server
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

target_link_libraries(ft-client
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

endif()
