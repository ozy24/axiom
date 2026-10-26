# 0006: fmt and jsoncpp are vendored; no vcpkg

**Decided:** 2026-10-26

id's `vcpkg.json` names fmt and jsoncpp without versions. fmt 11 and later reject
id's non-const `fmt::formatter<edict_t>::format()` (`g_local.h`), so an unpinned
vcpkg build breaks. Vendoring fmt 10.2.1 and jsoncpp 1.9.6 gives Windows and
Linux identical dependencies, offline builds and no version drift. jsoncpp is
linked statically, so the Linux module has no distro library dependency.

**Reopen if:** a dependency needs a security fix (bump the vendored copy and
record it here).
