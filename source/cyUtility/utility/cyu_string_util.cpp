#include "cyu_string_util.h"

namespace cyclone
{
namespace string_util
{

//-------------------------------------------------------------------------------------
std::string size_to_string(float s)
{
	char temp[64] = { 0 };

	static const size_t KB = 1024;
	static const size_t MB = 1024 * 1024;
	static const size_t GB = 1024 * 1024 * 1024;

	if (s < KB) {
		std::snprintf(temp, 64, "%.2f ", s);
	}
	else if (s < MB) {
		std::snprintf(temp, 64, "%.2f KB", (float)s / (float)(KB));
	}
	else if (s < GB) {
		std::snprintf(temp, 64, "%.2f MB", (float)s / (float)(MB));
	}
	else {
		std::snprintf(temp, 64, "%.2f GB", (float)s / (float)(GB));
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
