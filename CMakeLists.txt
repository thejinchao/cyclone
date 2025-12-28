#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 3.15)

project(cyclone)

########
#system
########
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CY_SYS_LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CY_SYS_WINDOWS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(CY_SYS_MACOS TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
	set(CY_SYS_ANDROID TRUE)
else()
	message(FATAL_ERROR "Unknown target system \"${CMAKE_SYSTEM_NAME}\".")
endif()

########
#set cyclone files path
########
set(CY_SOURCE_CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyCore)
set(CY_SOURCE_EVENT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyEvent)
set(CY_SOURCE_NETWORK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyNetwork)
set(CY_SOURCE_CRYPT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyCrypt)
set(CY_SOURCE_UTILITY_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyUtility)

########
#check include file
########
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
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
check_include_file(sys/uio.h		CY_HAVE_SYS_UIO_H)
check_include_file(sys/eventfd.h	CY_HAVE_SYS_EVENTFD_H)

########
#check functions
########
include(CheckFunctionExists)
include(CheckCXXSourceCompiles)
check_function_exists(epoll_ctl			CY_HAVE_EPOLL)
check_function_exists(readv				CY_HAVE_READWRITE_V)
check_function_exists(pipe2				CY_HAVE_PIPE2)
check_function_exists(kqueue			CY_HAVE_KQUEUE)
check_function_exists(timerfd_create	CY_HAVE_TIMERFD)


########
#compiler flag
########
if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
else()
	set(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -g -std=c++11 -Wall -Wextra -Werror -Wconversion -Wno-unused-parameter	-Woverloaded-virtual -Wpointer-arith -Wshadow -Wwrite-strings -Wno-sign-conversion")
endif()

########
#set system library
########
set(CY_SYSTEM_LIBRARIES "")
if(CY_SYS_LINUX)
elseif(CY_SYS_WINDOWS)
	set(CY_SYSTEM_LIBRARIES ws2_32.lib shlwapi.lib winmm.lib)
elseif(CY_SYS_MACOS)
elseif(CY_SYS_ANDROID)
endif()

#defined in $ANDROID_SDK_ROOT/build/cmake/android.toolchain.cmake
if(CY_SYS_ANDROID)
unset(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM)
unset(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY)
unset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)
unset(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE)
endif()

########
#is log enable
########
set(CY_ENABLE_LOG TRUE)

########
#make configure files
########
set(CY_AUTO_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/source)
set(CY_AUTO_CONFIG_FILE ${CY_AUTO_INCLUDE_PATH}/cyclone_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/cyclone_config.h.in ${CY_AUTO_CONFIG_FILE})

########
#sub dictionary
########
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_subdirectory(source)
add_subdirectory(samples)
add_subdirectory(test)
