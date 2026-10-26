// Adapter: the one implementation of Host, over the engine's gi and level.
#include "g_local.h"

#include "core/host.h"

#include <string>

namespace
{
class GameHost final : public Host
{
  public:
	void Print(std::string_view text) override
	{
		gi.Com_Print(std::string(text).c_str());
	}

	int64_t TimeMs() const override
	{
		return level.time.milliseconds();
	}
};
} // namespace

Host &Core_GameHost()
{
	static GameHost host;
	return host;
}
