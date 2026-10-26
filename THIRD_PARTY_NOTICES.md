# Third-party notices

This project as a whole is licensed under the GNU General Public License,
version 2 (see `LICENSE`).

| Component | Where | Licence | Source |
|---|---|---|---|
| Quake II Rerelease game source | `rerelease/` | GPL-2.0, Copyright (c) ZeniMax Media Inc. | github.com/id-Software/quake2-rerelease-dll @ `8dc1fc9` |
| {fmt} 10.2.1 | `third_party/fmt-10.2.1/` | MIT (see its `LICENSE`) | github.com/fmtlib/fmt, tag `10.2.1` |
| JsonCpp 1.9.6 | `third_party/jsoncpp-1.9.6/` | MIT or public domain (see its `LICENSE`) | github.com/open-source-parsers/jsoncpp, tag `1.9.6` (`89e2973`) |
| POSIX entry-point shim and compat header | `platform/posix/` | GPL-2.0 | Adapted from Q2RED `game-rerelease/` (same author) |

Vendored components are unmodified except that their own build files were
removed. Retail game data is not part of this repository and must never be
added to it.
