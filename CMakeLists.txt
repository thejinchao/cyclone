#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

project(cyclone)

########
#system
########
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CY_SYS_LINUX TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CY_SYS_WINDOWS TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(CY_SYS_DARWIN TRUE)
endif()

########
#set cyclone files path
########
set(CY_SOURCE_CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyCore)
set(CY_SOURCE_EVENT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyEvent)
set(CY_SOURCE_NETWORK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyNetwork)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/../sdk)

########
#check include file
########
include(CheckIncludeFile)
check_include_file(unistd.h 		CY_HAVE_UNISTD_H)
check_include_file(inttypes.h 		CY_HAVE_INTTYPES_H)
check_include_file(limits.h 		CY_HAVE_LIMITS_H)
check_include_file(sys/types.h		CY_HAVE_SYS_TYPES_H)
check_include_file(sys/param.h		CY_HAVE_SYS_PARAM_H)
check_include_file(sys/mount.h		CY_HAVE_SYS_MOUNT_H)
check_include_file(sys/statvfs.h	CY_HAVE_SYS_STATVFS_H)
check_include_file(crypt.h			CY_HAVE_CRYPT_H)
check_include_file(sys/prctl.h		CY_HAVE_SYS_PRCTL_H)
check_include_file(sys/vfs.h		CY_HAVE_SYS_VFS_H)

########
#check functions
########
include(CheckFunctionExists)
check_function_exists(epoll_ctl	CY_HAVE_EPOLL)

########
#is log enable
########
set(CY_ENABLE_LOG TRUE)

########
#build info
########
string(TIMESTAMP CY_BUILD_TIME "%Y-%m-%d %H:%M:%S")

#make configure files
set(CY_AUTO_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/source)
set(CY_AUTO_CONFIG_FILE ${CY_AUTO_INCLUDE_PATH}/cyclone_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/cyclone_config.h.in ${CY_AUTO_CONFIG_FILE})

########
#compiler flag
########
if(MSVC)
	option(CY_USE_MULTITHREAD "Use Multi-thread /MT" OFF)
endif()

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
	if(CY_USE_MULTITHREAD)
		set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} /MTd")
		set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} /MT")
	endif()
else()
	set(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -g")
endif()

########
#sub dictionary
########
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_subdirectory(source)
add_subdirectory(test)
