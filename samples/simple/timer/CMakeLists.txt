#
#Copyright(C) thecodeway.com
#

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
)

add_executable(timer timer.cpp)

set_property(TARGET timer PROPERTY FOLDER "samples/simple/timer")

if(CY_SYS_WINDOWS)
target_link_libraries(timer
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	${JEMALLOC_LIBRARIES}
)

else()

target_link_libraries(timer
	cyclone
	${JEMALLOC_LIBRARIES}
	${PTHREAD_LIBRARIES}
)

endif()
