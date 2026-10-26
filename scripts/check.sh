#!/usr/bin/env bash
# Every check that needs no game data. Hosted CI runs exactly this, plus the
# Windows and Linux builds; run it before opening a PR.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

scripts/check_repo_hygiene.sh
python3 -I scripts/check_save_baseline.py
python3 -I scripts/check_name_leak.py
python3 -I scripts/check_mod_markers.py
scripts/check_format.sh
