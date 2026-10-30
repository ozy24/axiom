// Adapter: host file access over gi.GetExtension.
#include "g_local.h"

#include "core/host_files.h"
#include "core/host_files_ext.h"

namespace
{
const q2red_filesystem_api_v1_t *Extension()
{
	static bool asked = false;
	static const q2red_filesystem_api_v1_t *fs = nullptr;
	if (!asked)
	{
		asked = true;
		// Retail leaves the slot null: calling it would crash.
		if (gi.GetExtension)
		{
			auto *found =
			    static_cast<const q2red_filesystem_api_v1_t *>(gi.GetExtension(Q2RED_FILESYSTEM_EXTENSION_V1));
			if (found && found->version >= 1 && found->struct_size >= sizeof(q2red_filesystem_api_v1_t) &&
			    found->WriteFile)
				fs = found;
		}
	}
	return fs;
}
} // namespace

bool Core_HostFiles_Available()
{
	return Extension() != nullptr;
}

bool Core_HostFiles_Write(const std::string &game_path, std::string_view data)
{
	const q2red_filesystem_api_v1_t *fs = Extension();
	return fs && !game_path.empty() && fs->WriteFile(game_path.c_str(), data.data(), data.size()) == 1;
}

bool Core_HostFiles_Read(const std::string &game_path, std::string &data)
{
	const q2red_filesystem_api_v1_t *fs = Extension();
	if (!fs || !fs->LoadFile || game_path.empty())
		return false;
	void *buffer = nullptr;
	const int64_t length = fs->LoadFile(game_path.c_str(), &buffer);
	if (length < 0 || !buffer)
		return false;
	data.assign(static_cast<const char *>(buffer), static_cast<size_t>(length));
	if (fs->FreeFile)
		fs->FreeFile(buffer);
	return true;
}
