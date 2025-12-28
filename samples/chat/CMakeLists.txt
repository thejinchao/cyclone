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

add_executable(chat-server chat_server.cpp)
add_executable(chat-client chat_client.cpp)

set_property(TARGET chat-server PROPERTY FOLDER "samples/chat")
set_property(TARGET chat-client PROPERTY FOLDER "samples/chat")

target_link_libraries(chat-server
	cyclone
	${CY_SYSTEM_LIBRARIES}
)

target_link_libraries(chat-client
	cyclone
	${CY_SYSTEM_LIBRARIES}
)
