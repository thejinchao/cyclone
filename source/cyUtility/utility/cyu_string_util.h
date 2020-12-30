/*
Copyright(C) thecodeway.com
*/
#pragma once

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
