/*
Copyright(C) thecodeway.com
*/
#pragma once

#include <cyclone_config.h>

namespace cyclone
{

//initial adler32 value
#define INITIAL_ADLER	(1u)

/*
* Update a running Adler-32 checksum with the bytes buf[0..len-1] and
* return the updated checksum.  If buf is NULL, this function returns the
* required initial value for the checksum.
* 
* An Adler-32 checksum is almost as reliable as a CRC32 but can be computed
* much faster.
* 
* Usage example:
* 
* uint32_t adler = cyclone::INITIAL_ADLER;
* 
* while (read_buffer(buffer, length) != EOF) {
*   adler = cyclone::adler32(adler, buffer, length);
* }
*
*/
uint32_t adler32(uint32_t adler, const uint8_t* buf, size_t len);

}
