#include "core/maps.h"

#include "doctest.h"

#include <algorithm>
#include <map>

namespace
{
std::vector<std::string> Maps(int n)
{
	std::vector<std::string> out;
	for (int i = 1; i <= n; ++i)
		out.push_back("q2dm" + std::to_string(i));
	return out;
}

bool SameMaps(std::vector<std::string> a, std::vector<std::string> b)
{
	std::sort(a.begin(), a.end());
	std::sort(b.begin(), b.end());
	return a == b;
}

// Plays `passes` passes from `order` through Core_Maps_Next and returns the
// maps in the order they came up.
std::vector<std::string> Play(std::vector<std::string> order, int passes)
{
	std::vector<std::string> played{order.front()};
	for (size_t i = 1; i < order.size() * passes; ++i)
	{
		const MapsNext next = Core_Maps_Next(order, played.back(), true);
		order = next.order;
		played.push_back(next.map);
	}
	return played;
}
} // namespace

TEST_CASE("maps: list words are lowercased, deduplicated, and unsafe ones dropped")
{
	CHECK(Core_Maps_Words("  Q2DM1 q64/DM1\tq2dm1 bad;map \"x\" ../up /root a//b end/ ok_map-2 ") ==
	      std::vector<std::string>{"q2dm1", "q64/dm1", "ok_map-2"});
	CHECK(Core_Maps_Words(std::string(33, 'a')).empty());
	CHECK(Core_Maps_Words("").empty());
}

TEST_CASE("maps: without shuffle the order is the list as written")
{
	std::mt19937 rng(1);
	const auto list = Maps(4);
	CHECK(Core_Maps_Order(list, {"q2dm4", "q2dm3", "q2dm2", "q2dm1"}, "q2dm3", false, rng) == list);
}

TEST_CASE("maps: a shuffled order is kept while it holds the list's maps")
{
	std::mt19937 rng(1);
	const auto list = Maps(4);
	const std::vector<std::string> kept{"q2dm3", "q2dm1", "q2dm4", "q2dm2"};
	CHECK(Core_Maps_Order(list, kept, "q2dm1", true, rng) == kept);

	// the list changed (an admin's edit, another gametype's list): a new order
	const auto five = Maps(5);
	const auto fresh = Core_Maps_Order(five, kept, "q2dm1", true, rng);
	CHECK(SameMaps(fresh, five));
}

TEST_CASE("maps: a new shuffled order starts with the current map, so its whole pass follows")
{
	const auto list = Maps(8);
	bool varied = false;
	std::vector<std::string> first;
	for (uint32_t seed = 0; seed < 50; ++seed)
	{
		std::mt19937 rng(seed);
		const auto order = Core_Maps_Order(list, {}, "Q2DM5", true, rng);
		CHECK(SameMaps(order, list));
		CHECK(order.front() == "q2dm5");
		if (first.empty())
			first = order;
		varied |= order != first;
	}
	CHECK(varied); // not the written order every time

	// a starting map that isn't listed: just shuffled
	std::mt19937 rng(3);
	CHECK(SameMaps(Core_Maps_Order(list, {}, "q2ctf1", true, rng), list));
}

TEST_CASE("maps: next follows the order; off the list the first; at the end it wraps")
{
	const std::vector<std::string> order{"q2dm3", "q2dm1", "q2dm7"};
	CHECK(Core_Maps_Next(order, "q2dm3", false).map == "q2dm1");
	CHECK(Core_Maps_Next(order, "Q2DM1", false).map == "q2dm7"); // case doesn't matter
	CHECK(Core_Maps_Next(order, "q2dm7", false).map == "q2dm3");
	CHECK(Core_Maps_Next(order, "q2dm7", false).order == order);
	// the server's starting map, or an admin's `map`, off the list: the first,
	// where stock would follow the map's own exit
	CHECK(Core_Maps_Next(order, "q2dm5", false).map == "q2dm3");
	CHECK(Core_Maps_Next(order, "q2dm5", true).map == "q2dm3");
	CHECK(Core_Maps_Next({"q2dm2"}, "q2dm2", true).map == "q2dm2");
	CHECK(Core_Maps_Next({}, "q2dm2", true).map.empty());
}

TEST_CASE("maps: the last map of a shuffled pass starts a reshuffled one, the same every time asked")
{
	const std::vector<std::string> order{"q2dm3", "q2dm1", "q2dm7", "q2dm2", "q2dm5"};
	const MapsNext next = Core_Maps_Next(order, "q2dm5", true);
	CHECK(SameMaps(next.order, order));
	CHECK(next.map == next.order.front());
	CHECK(next.map != "q2dm5");
	// the Tab menu asks before the level ends; the level end must agree
	CHECK(Core_Maps_Next(order, "q2dm5", true).order == next.order);
}

TEST_CASE("maps: every map plays once per pass, and waits at least half the list between plays")
{
	for (int n = 1; n <= 12; ++n)
	{
		for (uint32_t seed = 0; seed < 40; ++seed)
		{
			std::mt19937 rng(seed);
			const auto start = Core_Maps_Order(Maps(n), {}, "", true, rng);
			const auto played = Play(start, 6);
			for (int pass = 0; pass < 6; ++pass)
				CHECK(SameMaps({played.begin() + pass * n, played.begin() + (pass + 1) * n}, Maps(n)));
			std::map<std::string, size_t> last;
			for (size_t i = 0; i < played.size(); ++i)
			{
				if (const auto it = last.find(played[i]); it != last.end())
					CHECK(i - it->second - 1 >= (size_t)n / 2); // maps between its two plays
				last[played[i]] = i;
			}
		}
	}
}

TEST_CASE("maps: reshuffles vary, and only the recent maps are held back")
{
	const auto last = Maps(8); // just played q2dm1 .. q2dm8
	std::map<std::string, int> firsts;
	for (uint32_t seed = 0; seed < 400; ++seed)
	{
		std::mt19937 rng(seed);
		const auto next = Core_Maps_Reshuffle(last, rng);
		CHECK(SameMaps(next, last));
		// q2dm8 just played: 4 maps before it comes back, so not in slots 0-3
		CHECK(std::find(next.begin(), next.end(), "q2dm8") - next.begin() >= 4);
		++firsts[next.front()];
	}
	// the four that played longest ago can open the next pass; q2dm5 needs
	// three before it (four between its plays), so none of the last four can
	for (const char *m : {"q2dm1", "q2dm2", "q2dm3", "q2dm4"})
		CHECK(firsts[m] > 0);
	for (const char *m : {"q2dm5", "q2dm6", "q2dm7", "q2dm8"})
		CHECK(firsts.count(m) == 0);
}

TEST_CASE("maps: two maps alternate, three never repeat back to back")
{
	std::mt19937 rng(7);
	CHECK(Core_Maps_Reshuffle({"a", "b"}, rng) == std::vector<std::string>{"a", "b"});
	CHECK(Play({"a", "b"}, 3) == std::vector<std::string>{"a", "b", "a", "b", "a", "b"});
	for (uint32_t seed = 0; seed < 50; ++seed)
	{
		std::mt19937 r(seed);
		CHECK(Core_Maps_Reshuffle({"a", "b", "c"}, r).front() != "c");
	}
}
