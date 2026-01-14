#
#Copyright(C) thecodeway.com
#

########
#cyCore
########

set(CY_CORE_INCLUDE_FILES
	cyCore/cy_core.h
	cyCore/core/cyc_logger.h
	cyCore/core/cyc_socket_api.h
	cyCore/core/cyc_system_api.h
	cyCore/core/cyc_ring_buf.h
	cyCore/core/cyc_atomic.h
	cyCore/core/cyc_lf_queue.h
)
source_group("cyCore" FILES ${CY_CORE_INCLUDE_FILES})

set(CY_CORE_SOURCE_FILES 
	cyCore/core/cyc_logger.cpp
	cyCore/core/cyc_socket_api.cpp
	cyCore/core/cyc_system_api.cpp
	cyCore/core/cyc_ring_buf.cpp
)
source_group("cyCore" FILES ${CY_CORE_SOURCE_FILES})

########
#cyEvent
########

set(CY_EVENT_INCLUDE_FILES
	cyEvent/cy_event.h
	cyEvent/event/cye_looper.h
	cyEvent/event/cye_pipe.h
	cyEvent/event/cye_work_thread.h
	cyEvent/event/cye_packet.h
)
source_group("cyEvent" FILES ${CY_EVENT_INCLUDE_FILES})

set(CY_EVENT_SOURCE_FILES
	cyEvent/event/cye_looper.cpp
	cyEvent/event/cye_pipe.cpp
	cyEvent/event/cye_work_thread.cpp
	cyEvent/event/cye_packet.cpp
)
source_group("cyEvent" FILES ${CY_EVENT_SOURCE_FILES})


if(CY_HAVE_EPOLL)
set(CY_EVENT_INTERNAL_FILES
	cyEvent/event/internal/cye_looper_epoll.h
	cyEvent/event/internal/cye_looper_epoll.cpp
	cyEvent/event/internal/cye_create_looper.cpp
)
elseif(CY_HAVE_KQUEUE)
set(CY_EVENT_INTERNAL_FILES
	cyEvent/event/internal/cye_looper_kqueue.h
	cyEvent/event/internal/cye_looper_kqueue.cpp
	cyEvent/event/internal/cye_create_looper.cpp
)
else()
set(CY_EVENT_INTERNAL_FILES
	cyEvent/event/internal/cye_looper_select.h
	cyEvent/event/internal/cye_looper_select.cpp
	cyEvent/event/internal/cye_create_looper.cpp
)
endif()
source_group("cyEvent\\internal" FILES ${CY_EVENT_INTERNAL_FILES})

########
#cyNetwork
########

set(CY_NETWORK_INCLUDE_FILES
	cyNetwork/cy_network.h
	cyNetwork/network/cyn_address.h
	cyNetwork/network/cyn_tcp_server.h
	cyNetwork/network/cyn_tcp_connection.h
	cyNetwork/network/cyn_tcp_client.h
)
source_group("cyNetwork" FILES ${CY_NETWORK_INCLUDE_FILES})

set(CY_NETWORK_SOURCE_FILES
	cyNetwork/network/cyn_address.cpp
	cyNetwork/network/cyn_tcp_server.cpp
	cyNetwork/network/cyn_tcp_connection.cpp
	cyNetwork/network/cyn_tcp_client.cpp
)
source_group("cyNetwork" FILES ${CY_NETWORK_SOURCE_FILES})

set(CY_NETWORK_INTERNAL_FILES
	cyNetwork/network/internal/cyn_tcp_server_master_thread.h
	cyNetwork/network/internal/cyn_tcp_server_work_thread.h
	cyNetwork/network/internal/cyn_tcp_server_master_thread.cpp
	cyNetwork/network/internal/cyn_tcp_server_work_thread.cpp
)
source_group("cyNetwork\\internal" FILES ${CY_NETWORK_INTERNAL_FILES})

########
#cyCrypt
########
set(CY_CRYPT_INCLUDE_FILES
	cyCrypt/cy_crypt.h
	cyCrypt/crypt/cyr_adler32.h
	cyCrypt/crypt/cyr_dhexchange.h
	cyCrypt/crypt/cyr_xorshift128.h
	cyCrypt/crypt/cyr_rijndael.h
)
source_group("cyCrypt" FILES ${CY_CRYPT_INCLUDE_FILES})

set(CY_CRYPT_SOURCE_FILES
	cyCrypt/crypt/cyr_adler32.cpp
	cyCrypt/crypt/cyr_dhexchange.cpp
	cyCrypt/crypt/cyr_xorshift128.cpp
	cyCrypt/crypt/cyr_rijndael.cpp
)
source_group("cyCrypt" FILES ${CY_CRYPT_SOURCE_FILES})

########
#cyUtility
########
set(CY_UTILITY_INCLUDE_FILES
	cyUtility/utility/cyu_statistics.h
	cyUtility/utility/cyu_simple_opt.h
	cyUtility/utility/cyu_ring_queue.h
	cyUtility/utility/cyu_string_util.h
)
source_group("cyUtility" FILES ${CY_UTILITY_INCLUDE_FILES})

set(CY_UTILITY_SOURCE_FILES
	cyUtility/utility/cyu_string_util.cpp
)
source_group("cyUtility" FILES ${CY_UTILITY_SOURCE_FILES})

include_directories(
	${CY_AUTO_INCLUDE_PATH}
	${CY_SOURCE_CORE_PATH}
	${CY_SOURCE_EVENT_PATH}
	${CY_SOURCE_NETWORK_PATH}
	${CY_SOURCE_CRYPT_PATH}
	${CY_SOURCE_UTILITY_PATH}
)

add_library(cyclone 
	${CY_CORE_SOURCE_FILES} 
	${CY_CORE_INCLUDE_FILES} 
	${CY_EVENT_INCLUDE_FILES}
	${CY_EVENT_SOURCE_FILES}
	${CY_EVENT_INTERNAL_FILES}
	${CY_NETWORK_INCLUDE_FILES}
	${CY_NETWORK_SOURCE_FILES}
	${CY_NETWORK_INTERNAL_FILES}
	${CY_CRYPT_INCLUDE_FILES}
	${CY_CRYPT_SOURCE_FILES}
	${CY_UTILITY_INCLUDE_FILES}
	${CY_UTILITY_SOURCE_FILES}
)

set_target_properties(cyclone PROPERTIES
  VERSION ${CYCLONE_VERSION} SOVERSION ${CPACK_PACKAGE_VERSION_MAJOR}
  DEBUG_POSTFIX "${CYCLONE_DEBUG_POSTFIX}"
)

########
#INSTALL
########
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(version_config ${PROJECT_BINARY_DIR}/cyclone-config-version.cmake)
set(project_config ${PROJECT_BINARY_DIR}/cyclone-config.cmake)
set(targets_export_name cyclone-targets)
set(CYCLONE_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/cyclone)

# generate the version, config and target files into the build directory.
write_basic_package_version_file(
	${version_config}
	VERSION ${CYCLONE_VERSION}
	COMPATIBILITY AnyNewerVersion
)

# generate the project config file.
configure_package_config_file(
	${PROJECT_SOURCE_DIR}/cmake/cyclone-config.cmake.in
	${project_config}
	INSTALL_DESTINATION ${CYCLONE_CMAKE_INSTALL_DIR}
	PATH_VARS CMAKE_INSTALL_PREFIX 
)

# install the library and headers.
install(FILES ${CY_AUTO_CONFIG_FILE}
	DESTINATION include)
	
install(DIRECTORY cyCore/ cyEvent/ cyNetwork/ cyCrypt/ cyUtility/
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "internal" EXCLUDE)

install(TARGETS cyclone 
	EXPORT ${targets_export_name}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# imported targets.
export(TARGETS cyclone NAMESPACE cyclone::
	FILE ${PROJECT_BINARY_DIR}/${targets_export_name}.cmake
)

# install version, config and target files.
install(
	FILES ${project_config} ${version_config}
	DESTINATION ${CYCLONE_CMAKE_INSTALL_DIR}
)

install(EXPORT ${targets_export_name}
	DESTINATION ${CYCLONE_CMAKE_INSTALL_DIR}
	NAMESPACE cyclone::
)
