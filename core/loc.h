// Location callouts (`loc`, decision 0027), as pure logic. The adapter is
// loc_register.cpp.
//
//   bind c "cmd loc ENEMY - %l"      ->  (Alice): ENEMY - [Red Armor]
//   bind e "cmd loc STATUS %h %a %l" ->  (Alice): STATUS H:87 A:50 [Red Armor]
//
// The place comes from the map's .loc file (the community format: x y z,
// eight times the map's coordinates, then a name), or with none, from the
// nearest weapon, powerup or mega health in view. The tokens and their text
// are MuffMode's, so its players' binds work unchanged.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct LocVec
{
	float x = 0, y = 0, z = 0;
};

struct LocPlace
{
	LocVec origin; // map units (the file's divided by 8)
	std::string label;
};

constexpr size_t kLocMaxPlaces = 4096;
constexpr size_t kLocMaxLabel = 64;
constexpr size_t kLocMaxText = 150;    // the whole callout, as chat's line cap
constexpr size_t kLocVisibleTries = 8; // the nearest places checked for a line of sight
constexpr size_t kLocMaxNames = 4;     // %n / %N list this many, then "+2"

// A place name as shown: control characters and runs of white space become
// one space, trimmed, at most kLocMaxLabel bytes.
std::string Core_Loc_CleanLabel(std::string_view label);

// One line of a .loc file, "<x> <y> <z> <name...>". False for a blank,
// comment or malformed line.
bool Core_Loc_ParseLine(std::string_view line, LocPlace &out);

// Every place in a .loc file, at most kLocMaxPlaces. `skipped` counts the
// lines that held something but didn't parse.
std::vector<LocPlace> Core_Loc_ParseFile(std::string_view text, size_t &skipped);

// The place to name for someone at `from`: the nearest of the
// kLocVisibleTries nearest that `seen` accepts (a line of sight), else the
// nearest. -1 with no places.
int Core_Loc_Pick(const std::vector<LocPlace> &places, LocVec from, const std::function<bool(const LocPlace &)> &seen);

// An item's short name by its classname: MuffMode's "RG", "QUAD", "MEGA";
// else the classname after its "weapon_" / "ammo_" / "item_", in capitals
// ("BLASTER", "SLUGS"). Never the pickup name: that is a $loc key, which
// the client translates only as a whole print argument.
std::string Core_Loc_ItemName(std::string_view classname);

// The item fallback's label: "upper RG" when every other of the same item
// (`twins`, their heights) is lower, "lower RG" when every other is higher,
// else plain "RG".
std::string Core_Loc_ItemLabel(std::string_view name, float z, const std::vector<float> &twins);

// "A:50", or "A:50 P:80" with power armor (`cells` < 0: none).
std::string Core_Loc_ArmorText(int armor, int cells);

// "Bob, Carol", at most kLocMaxNames, then " +N"; "nobody" for none.
std::string Core_Loc_Names(const std::vector<std::string> &names);

// Whether `target` is in front of an eye at `eye` looking along `forward`
// (a unit vector): within the same cone as stock's infront().
bool Core_Loc_InView(LocVec eye, LocVec forward, LocVec target);

// The callout's text from the command's arguments: trimmed, outer quotes
// dropped. Empty when it holds no token (`loc` is not a second chat).
std::string Core_Loc_Text(std::string_view args);

// Whether `text` uses token `c` ("%c").
bool Core_Loc_Uses(std::string_view text, char c);

struct LocValues
{
	std::string place;  // %l %L: "[place]"
	int health = 0;     // %h: "H:87"
	std::string armor;  // %a: Core_Loc_ArmorText
	std::string weapon; // %w: "RG:25", "BLASTER"
	std::string ammo;   // %m: "SLUGS"
	std::string team;   // %n: teammates in view (Core_Loc_Names)
	std::string all;    // %N: players in view
};

// `text` with each token replaced, in one pass (a place named "%h" stays
// "%h"), cut to kLocMaxText. Anything else after a '%' is left as typed.
std::string Core_Loc_Expand(std::string_view text, const LocValues &values);
