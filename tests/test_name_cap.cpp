#include "core/name_cap.h"

#include "doctest.h"

TEST_CASE("names: cut to 15 characters")
{
	CHECK(Core_Name_Cap("Alice") == "Alice");
	CHECK(Core_Name_Cap("") == "");
	CHECK(Core_Name_Cap("123456789012345") == "123456789012345");
	CHECK(Core_Name_Cap("CRT_SECURE_NO_WARNINGS") == "CRT_SECURE_NO_W");
	CHECK(Core_Name_Cap("##P3") == "##P3");
}

TEST_CASE("names: the cut never splits a UTF-8 character")
{
	// 14 ASCII then a two-byte é then more: the é is the 15th character.
	CHECK(Core_Name_Cap("abcdefghijklmn\xC3\xA9xyz") == "abcdefghijklmn\xC3\xA9");
	// 15 three-byte characters fit whole; a 16th is cut.
	std::string sixteen;
	for (int i = 0; i < 16; ++i)
		sixteen += "\xE2\x98\x85";
	CHECK(Core_Name_Cap(sixteen) == sixteen.substr(0, 45));
	CHECK(Core_Name_Cap("ab", 0) == "");
}
