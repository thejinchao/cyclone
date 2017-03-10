#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.8)

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
	set(CY_SYS_MACOS TRUE)
endif()

########
#set cyclone files path
########
set(CY_SOURCE_CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyCore)
set(CY_SOURCE_EVENT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyEvent)
set(CY_SOURCE_NETWORK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyNetwork)
set(CY_SOURCE_CRYPT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cyCrypt)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/../sdk)

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

if(MSVC)
check_include_file_cxx(atomic		CY_HAVE_ATOMIC_H)
else()
check_include_file_cxx(atomic		CY_HAVE_ATOMIC_H "-std=c++0x")
endif()

########
#check functions
########
include(CheckFunctionExists)
check_function_exists(epoll_ctl	CY_HAVE_EPOLL)
check_function_exists(readv CY_HAVE_READWRITE_V)
check_function_exists(pipe2 CY_HAVE_PIPE2)
check_function_exists(kqueue CY_HAVE_KQUEUE)
check_function_exists(timerfd_create CY_HAVE_TIMERFD)


########
#find jemalloc library
########
find_path(JEMALLOC_SDK_ROOT
    NAMES include/jemalloc/jemalloc.h
    HINTS $ENV{JEMALLOC_SDK_ROOT}
)
find_library(JEMALLOC_LIBRARIES
    NAMES jemalloc_pic
    HINTS ${JEMALLOC_SDK_ROOT}/lib
)
find_path(JEMALLOC_INCLUDE_DIR
    NAMES jemalloc/jemalloc.h
    HINTS ${JEMALLOC_SDK_ROOT}/include
)

if(JEMALLOC_SDK_ROOT AND JEMALLOC_LIBRARIES)
  set(CY_HAVE_JEMALLOC_LIBRARIES TRUE)
else()
  set(CY_HAVE_JEMALLOC_LIBRARIES FALSE)
  set(JEMALLOC_LIBRARIES "")
endif()

mark_as_advanced(
	JEMALLOC_LIBRARIES
	JEMALLOC_INCLUDE_DIR
)

if(CY_HAVE_JEMALLOC_LIBRARIES)
include_directories(
	${JEMALLOC_INCLUDE_DIR}
)
endif()

########
#find gtest and gmock library
########
find_package(GTest)

########
#is log enable
########
set(CY_ENABLE_LOG TRUE)

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
		set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} /MTd /W4 /ZI")
		set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} /MT /W4 /Zi")
	else()
		set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} /MDd /W4 /ZI")
		set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} /MD /W4 /Zi")
	endif()
else()
	set(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -g -O2 -std=c++0x -Wall -Wextra -Werror -Wconversion -Wno-unused-parameter	-Woverloaded-virtual -Wpointer-arith -Wshadow -Wwrite-strings -Wno-deprecated")
endif()

########
#sub dictionary
########
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_subdirectory(source)
add_subdirectory(samples)

########
#test dictionary
########
if(GTEST_FOUND)
    enable_testing()
    add_subdirectory(test)
endif()
