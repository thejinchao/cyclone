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

add_executable(timer timer.cpp)

set_property(TARGET timer PROPERTY FOLDER "samples/timer")

target_link_libraries(timer
	cyclone
	${CY_SYSTEM_LIBRARIES}
)
