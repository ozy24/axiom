// Pure part of the name cap (core/name_cap.h). No engine access.
#include "core/name_cap.h"

std::string Core_Name_Cap(const std::string &name, size_t max_chars)
{
	size_t chars = 0;
	for (size_t i = 0; i < name.size(); ++i)
	{
		// A character starts at any byte that isn't a UTF-8 continuation byte.
		if ((static_cast<unsigned char>(name[i]) & 0xC0) != 0x80 && chars++ == max_chars)
			return name.substr(0, i);
	}
	return name;
}
