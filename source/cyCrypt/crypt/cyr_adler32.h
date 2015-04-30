/*
Copyright(C) thecodeway.com
*/
#ifndef _CYCLONE_CRYPT_ADLER_H_
#define _CYCLONE_CRYPT_ADLER_H_

#include <cyclone_config.h>

namespace cyclone
{

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
* uint32_t adler = cyclone::adler32(0, 0, 0);
* 
* while (read_buffer(buffer, length) != EOF) {
*   adler = cyclone::adler32(adler, buffer, length);
* }
*
*/
uint32_t adler32(uint32_t adler, const char* buf, size_t len);

}

#endif

