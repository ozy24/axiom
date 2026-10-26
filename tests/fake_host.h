// A Host for unit tests: records what the module prints, time is set by the test.
#pragma once

#include "core/host.h"

#include <string>
#include <vector>

class FakeHost final : public Host
{
  public:
	void Print(std::string_view text) override
	{
		printed.emplace_back(text);
	}

	int64_t TimeMs() const override
	{
		return time_ms;
	}

	std::vector<std::string> printed;
	int64_t time_ms = 0;
};
