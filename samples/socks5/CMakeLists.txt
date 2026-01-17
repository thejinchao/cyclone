#
#Copyright(C) thecodeway.com
#

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${CY_SOURCE_UTILITY_PATH}
)

add_executable(socks5
    s5_main.cpp
    s5_protocol.h
    s5_protocol.cpp
)

set_property(TARGET socks5 PROPERTY FOLDER "samples/socks5")

target_link_libraries(socks5
	cyclone
	${CY_SYSTEM_LIBRARIES}
)
