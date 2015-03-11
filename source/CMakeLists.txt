#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

########
#cyCore
########

set(CY_CORE_INCLUDE_FILES
	cyCore/cy_core.h
	cyCore/core/cyc_logger.h
	cyCore/core/cyc_socket_api.h
	cyCore/core/cyc_thread_api.h
	cyCore/core/cyc_ring_buf.h
	cyCore/core/cyc_adler.h
	cyCore/core/cyc_atomic.h
)
source_group("cyCore" FILES ${CY_CORE_INCLUDE_FILES})

set(CY_CORE_SOURCE_FILES 
	cyCore/core/cyc_logger.cpp
	cyCore/core/cyc_socket_api.cpp
	cyCore/core/cyc_thread_api.cpp
	cyCore/core/cyc_ring_buf.cpp
	cyCore/core/cyc_adler.cpp
)
source_group("cyCore" FILES ${CY_CORE_SOURCE_FILES})

########
#cyEvent
########

set(CY_EVENT_INCLUDE_FILES
	cyEvent/cy_event.h
	cyEvent/event/cye_looper.h
	cyEvent/event/cye_pipe.h
)
source_group("cyEvent" FILES ${CY_EVENT_INCLUDE_FILES})

set(CY_EVENT_SOURCE_FILES
	cyEvent/event/cye_looper.cpp
	cyEvent/event/cye_pipe.cpp
)
source_group("cyEvent" FILES ${CY_EVENT_SOURCE_FILES})

set(CY_EVENT_INTERNAL_FILES
	cyEvent/event/internal/cye_looper_select.h
	cyEvent/event/internal/cye_looper_epoll.h
	cyEvent/event/internal/cye_looper_select.cpp
	cyEvent/event/internal/cye_looper_epoll.cpp
	cyEvent/event/internal/cye_create_looper.cpp
)
source_group("cyEvent\\internal" FILES ${CY_EVENT_INTERNAL_FILES})

########
#cyNetwork
########

set(CY_NETWORK_INCLUDE_FILES
	cyNetwork/cy_network.h
	cyNetwork/network/cyn_socket.h
	cyNetwork/network/cyn_address.h
	cyNetwork/network/cyn_tcp_server.h
	cyNetwork/network/cyn_connection.h
	cyNetwork/network/cyn_work_thread.h
)
source_group("cyNetwork" FILES ${CY_NETWORK_INCLUDE_FILES})

set(CY_NETWORK_SOURCE_FILES
	cyNetwork/network/cyn_socket.cpp
	cyNetwork/network/cyn_address.cpp
	cyNetwork/network/cyn_tcp_server.cpp
	cyNetwork/network/cyn_connection.cpp
	cyNetwork/network/cyn_work_thread.cpp
)
source_group("cyNetwork" FILES ${CY_NETWORK_SOURCE_FILES})

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
)

add_library(cyclone 
	${CY_CORE_SOURCE_FILES} 
	${CY_CORE_INCLUDE_FILES} 
	${CY_EVENT_INCLUDE_FILES}
	${CY_EVENT_SOURCE_FILES}
	${CY_EVENT_INTERNAL_FILES}
	${CY_NETWORK_INCLUDE_FILES}
	${CY_NETWORK_SOURCE_FILES}
)

########
#INSTALL
########
install(FILES ${CY_AUTO_CONFIG_FILE}
	DESTINATION include)
	
install(DIRECTORY cyCore/ cyEvent/ cyNetwork/
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "internal" EXCLUDE)

if (MSVC)
install(TARGETS cyclone
	DESTINATION lib/Release
	CONFIGURATIONS Release)
	
install(TARGETS cyclone
	DESTINATION lib/Debug
	CONFIGURATIONS Debug)	
else()

install(TARGETS cyclone
	DESTINATION lib)

endif()
