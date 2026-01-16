#include "cyu_string_util.h"

namespace cyclone
{
namespace string_util
{

//-------------------------------------------------------------------------------------
std::string size_to_string(float s)
{
	char temp[64] = { 0 };

	static const double KB = 1024.0;
	static const double MB = 1024.0 * 1024.0;
	static const double GB = 1024.0 * 1024.0 * 1024.0;
	const double ds = static_cast<double>(s);

	if (ds < KB) {
		std::snprintf(temp, 64, "%.2f ", ds);
	}
	else if (ds < MB) {
		std::snprintf(temp, 64, "%.2f KB", ds / KB);
	}
	else if (ds < GB) {
		std::snprintf(temp, 64, "%.2f MB", ds / MB);
	}
	else {
		std::snprintf(temp, 64, "%.2f GB", ds / GB);
	}
	
	return std::string(temp);
}

//-------------------------------------------------------------------------------------
std::string size_to_string(size_t s)
{
	return size_to_string((float)s);
}

}
}
