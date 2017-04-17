#
#Copyright(C) thecodeway.com
#

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_CRYPT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${CY_UTILITY_PATH}/SimpleOpt
)
add_executable(relay_local
    relay_local.cpp
	relay_protocol.h
)

add_executable(relay_server
    relay_server.cpp
	relay_protocol.h
)

add_executable(relay_pipe
    relay_pipe.cpp
)

set_property(TARGET relay_local PROPERTY FOLDER "samples/relay")
set_property(TARGET relay_server PROPERTY FOLDER "samples/relay")
set_property(TARGET relay_pipe PROPERTY FOLDER "samples/relay")

if(CY_SYS_WINDOWS)
target_link_libraries(relay_local
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

target_link_libraries(relay_server
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

target_link_libraries(relay_pipe
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)
else()

target_link_libraries(relay_local
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

target_link_libraries(relay_server
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

target_link_libraries(relay_pipe
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)
endif()
