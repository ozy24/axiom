// Pure part of the host file access: the game-path rule. No engine access.
#include "core/host_files.h"

namespace
{
// Q2RED's rule (A62): every character, and no empty or dot-led component.
bool ValidGamePath(std::string_view path)
{
	if (path.empty() || path.size() >= 64 || path.back() == '/')
		return false;
	for (size_t i = 0; i < path.size(); ++i)
	{
		const char c = path[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
		      c == '.' || c == '/'))
			return false;
		const bool component_start = i == 0 || path[i - 1] == '/';
		if (component_start && (c == '/' || c == '.'))
			return false;
	}
	return true;
}
} // namespace

std::string Core_GamePath(std::string_view dir, std::string_view file)
{
	while (!dir.empty() && dir.back() == '/')
		dir.remove_suffix(1);
	if (dir.empty() || file.empty())
		return {};
	std::string path(dir);
	path += '/';
	path += file;
	return ValidGamePath(path) ? path : std::string();
}
