/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_UTILITY_STRING_UTIL_H_
#define _CYCLONE_UTILITY_STRING_UTIL_H_

#include <cyclone_config.h>

namespace cyclone
{
namespace string_util
{

// convert size to string with 'KB', 'MB', 'GB' tail
std::string size_to_string(float s);
std::string size_to_string(size_t s);

}
}

#endif
