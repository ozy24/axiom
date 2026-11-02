#include "doctest.h"

#include "core/admin.h"

#include <set>
#include <string>

TEST_CASE("admin: secrets compare whole")
{
	CHECK(Core_Admin_SecretEquals("hunter2", "hunter2"));
	CHECK_FALSE(Core_Admin_SecretEquals("hunter2", "hunter3"));
	CHECK_FALSE(Core_Admin_SecretEquals("hunter2", "hunter"));
	CHECK_FALSE(Core_Admin_SecretEquals("hunter", "hunter2"));
	CHECK_FALSE(Core_Admin_SecretEquals("", "x"));
	CHECK(Core_Admin_SecretEquals("", ""));
}

TEST_CASE("admin: no password is off, and checks nothing")
{
	Admins a;
	CHECK(a.Login("id:1", "", "", 0) == AdminLogin::Off);
	CHECK(a.Login("id:1", "anything", "", 0) == AdminLogin::Off);
	CHECK_FALSE(a.IsAdmin("id:1"));
	CHECK(a.TriesLeft("id:1", 0) == 3);
}

TEST_CASE("admin: the right password makes an admin; again says so")
{
	Admins a;
	CHECK(a.Login("id:1", "pw", "pw", 0) == AdminLogin::Ok);
	CHECK(a.IsAdmin("id:1"));
	CHECK_FALSE(a.IsAdmin("id:2"));
	CHECK(a.Login("id:1", "pw", "pw", 10) == AdminLogin::Already);
}

TEST_CASE("admin: three wrong guesses lock the player out for ten minutes")
{
	Admins a;
	CHECK(a.Login("id:1", "a", "pw", 0) == AdminLogin::Wrong);
	CHECK(a.TriesLeft("id:1", 0) == 2);
	CHECK(a.Login("id:1", "b", "pw", 1000) == AdminLogin::Wrong);
	CHECK(a.Login("id:1", "c", "pw", 2000) == AdminLogin::Wrong);
	CHECK(a.TriesLeft("id:1", 2000) == 0);
	CHECK(a.LockedForMs("id:1", 2000) == 600000);
	// Locked: even the right one isn't checked, and the lockout doesn't grow.
	CHECK(a.Login("id:1", "pw", "pw", 60000) == AdminLogin::LockedOut);
	CHECK(a.LockedForMs("id:1", 60000) == 542000);
	CHECK_FALSE(a.IsAdmin("id:1"));
	// Someone else isn't locked.
	CHECK(a.Login("id:2", "pw", "pw", 60000) == AdminLogin::Ok);
	// Over: three tries again.
	CHECK(a.LockedForMs("id:1", 602000) == 0);
	CHECK(a.TriesLeft("id:1", 602000) == 3);
	CHECK(a.Login("id:1", "pw", "pw", 602000) == AdminLogin::Ok);
}

TEST_CASE("admin: wrong guesses spread out are forgotten; a login clears them")
{
	Admins a;
	CHECK(a.Login("id:1", "a", "pw", 0) == AdminLogin::Wrong);
	CHECK(a.Login("id:1", "b", "pw", 1000) == AdminLogin::Wrong);
	// Ten minutes after the last: forgotten.
	CHECK(a.Login("id:1", "c", "pw", 601000) == AdminLogin::Wrong);
	CHECK(a.TriesLeft("id:1", 601000) == 2);
	CHECK(a.Login("id:1", "pw", "pw", 602000) == AdminLogin::Ok);
	CHECK(a.TriesLeft("id:1", 602000) == 3);
}

TEST_CASE("admin: an admin stays one through a short absence, not a long one")
{
	Admins a;
	REQUIRE(a.Login("id:1", "pw", "pw", 0) == AdminLogin::Ok);
	CHECK(a.Present({"id:1"}, 1000).empty());
	// A map change: gone for a few seconds, then back.
	CHECK(a.Present({}, 2000).empty());
	CHECK(a.Present({}, 20000).empty());
	CHECK(a.Present({"id:1"}, 21000).empty());
	CHECK(a.IsAdmin("id:1"));
	// The grace starts again from the next time they go.
	CHECK(a.Present({}, 100000).empty());
	CHECK(a.Present({}, 160000).empty());
	CHECK(a.Present({}, 160001) == std::vector<std::string>{"id:1"});
	CHECK_FALSE(a.IsAdmin("id:1"));
}

TEST_CASE("admin: a slot key goes the moment it's gone")
{
	Admins a;
	REQUIRE(a.Login("slot:3", "pw", "pw", 0) == AdminLogin::Ok);
	CHECK(a.Present({"slot:3"}, 100).empty());
	CHECK(a.Present({}, 200) == std::vector<std::string>{"slot:3"});
	CHECK_FALSE(a.IsAdmin("slot:3"));
}

TEST_CASE("admin: a new password logs everyone out, keeping the lockouts")
{
	Admins a;
	REQUIRE(a.Login("id:1", "pw", "pw", 0) == AdminLogin::Ok);
	for (int i = 0; i < 3; ++i)
		a.Login("id:2", "x", "pw", 0);
	a.LogoutAll();
	CHECK_FALSE(a.IsAdmin("id:1"));
	CHECK(a.LockedForMs("id:2", 1000) > 0);
	CHECK(a.Login("id:2", "new", "new", 1000) == AdminLogin::LockedOut);
}

TEST_CASE("admin: the state survives encoding; junk is skipped")
{
	Admins a;
	REQUIRE(a.Login("id:Steam:765", "pw", "pw", 0) == AdminLogin::Ok);
	a.Present({}, 5000); // gone since 5000
	a.Login("id:EOS:abc", "x", "pw", 7000);
	a.Login("id:EOS:abc", "y", "pw", 8000);
	a.Login("has space", "x", "pw", 8000); // not kept
	const std::string text = a.Encode();
	CHECK(text == "a:5000:id:Steam:765 t:2:8000:id:EOS:abc");
	const Admins b = Admins::Decode(text);
	CHECK(b.IsAdmin("id:Steam:765"));
	CHECK(b.TriesLeft("id:EOS:abc", 9000) == 1);
	CHECK(b.Encode() == text);
	// The grace counts from when they went, before the reload.
	Admins c = Admins::Decode(text);
	CHECK(c.Present({}, 65001) == std::vector<std::string>{"id:Steam:765"});

	const Admins junk = Admins::Decode("x a: a:12 a:zz:id:1 t:1:id:2 t:0:5:id:3  a:-:k a:7:id:4");
	CHECK(junk.Encode() == "a:7:id:4");
	CHECK(Admins::Decode("").Encode().empty());
}

TEST_CASE("admin: the command list")
{
	CHECK(Core_Admin_FindCommand("kick")->route == AdminRoute::Kick);
	CHECK(Core_Admin_FindCommand("MAP")->route == AdminRoute::Map);
	CHECK(Core_Admin_FindCommand("lockteam")->route == AdminRoute::Server);
	CHECK(Core_Admin_FindCommand("match")->look);
	CHECK_FALSE(Core_Admin_FindCommand("endmatch")->look);
	// The console's own power is never reachable.
	for (const char *no : {"set", "exec", "quit", "rcon", "addip", "writeip", "removeip", "listip", "votemap",
	                       "saveents", "gamemap", "", "admin"})
		CHECK_MESSAGE(Core_Admin_FindCommand(no) == nullptr, no);
	std::set<std::string> names;
	for (const AdminCommand &c : Core_Admin_Commands())
	{
		CHECK(names.insert(c.name).second);
		CHECK(Core_Admin_Help().find(std::string("  ") + c.name) != std::string::npos);
	}
}
