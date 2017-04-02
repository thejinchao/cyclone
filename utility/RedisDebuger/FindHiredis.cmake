# FindHiredis.cmake - Try to find the Hiredis library
# Once done this will define
#
#  HIREDIS_FOUND - System has Hiredis
#  HIREDIS_INCLUDE_DIR - The Hiredis include directory
#  HIREDIS_LIBRARIES - The libraries needed to use Hiredis
#  HIREDIS_DEFINITIONS - Compiler switches required for using Hiredis
#
# Usage(cyclone project):
# 
#   list(APPEND CMAKE_MODULE_PATH "${CY_UTILITY_PATH}/RedisDebuger")
#   find_package(Hiredis)
#   if(HIREDIS_FOUND)
#       set(CY_REDIS_DEBUGER_UTILITY 
#           ${CY_UTILITY_PATH}/RedisDebuger/redis_debugger.h
#           ${CY_UTILITY_PATH}/RedisDebuger/redis_debugger.cpp
#       )
#    
#       add_definitions(-DCY_HIREDIS_ENABLED)
#   endif()
#
#

#SET(HIREDIS_DEFINITIONS ${PC_HIREDIS_CFLAGS_OTHER})

FIND_PATH(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h
   HINTS
   $ENV{HIREDIS_SDK_ROOT}/include
   /usr
   /usr/local
   /opt
   PATH_SUFFIXES hiredis
   )

FIND_LIBRARY(HIREDIS_LIBRARIES NAMES hiredis
   HINTS
    $ENV{HIREDIS_SDK_ROOT}/lib
   /usr
   /usr/local
   /opt
   )


INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Hiredis DEFAULT_MSG HIREDIS_LIBRARIES HIREDIS_INCLUDE_DIR)

MARK_AS_ADVANCED(HIREDIS_INCLUDE_DIR HIREDIS_LIBRARIES)
