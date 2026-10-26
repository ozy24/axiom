#include "doctest.h"

#include "core/identity.h"
#include "fake_host.h"
#include "mod_identity.h"

#include <regex>

TEST_CASE("the version string is <name> <version> (<build>)")
{
	const std::string version = Core_VersionString();
	CHECK(version == std::string(MOD_NAME) + " " + MOD_VERSION + " (" + MOD_BUILD + ")");
	CHECK(std::regex_search(version, std::regex(R"( [0-9]+\.[0-9]+\.[0-9]+ \()")));
}

TEST_CASE("the banner prints the version string once, as one line")
{
	FakeHost host;
	Core_Identity_PrintBanner(host);
	REQUIRE(host.printed.size() == 1);
	CHECK(host.printed[0] == Core_VersionString() + "\n");
}
