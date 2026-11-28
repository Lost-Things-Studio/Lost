/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <zlib.h>

#include <OvTools/Utils/Checksum.h>

uint32_t OvTools::Utils::Crc32(const void* p_data, size_t p_size)
{
	uLong crc = crc32_z(0L, Z_NULL, 0);
	crc = crc32_z(crc, static_cast<const Bytef*>(p_data), p_size);
	return static_cast<uint32_t>(crc);
}
