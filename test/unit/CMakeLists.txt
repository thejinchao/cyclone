#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.8)

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${GTEST_INCLUDE_DIR}
)

link_directories(
    ${GTEST_LIBRARIES_PATH}
)

add_executable(cyt_unit 
    cyt_unit_main.cpp
)
    
set_property(TARGET cyt_unit PROPERTY FOLDER "test/unit")

if(CY_SYS_WINDOWS)
target_link_libraries(cyt_unit
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	gtest.lib
)

else()

target_link_libraries(cyt_unit
	cyclone
	${JEMALLOC_LIBRARIES}
	pthread
	gtest
)

endif()
