/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <cstdint>

namespace OvTools::Utils
{
	/**
	* The CRC-32 of bytes, as zlib and the ZIP format compute it: the check of each file of a package (.lopak) and of a
	* prepared model (.lomodel)
	* @param p_data
	* @param p_size
	*/
	uint32_t Crc32(const void* p_data, size_t p_size);
}
