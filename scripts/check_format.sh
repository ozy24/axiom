#!/usr/bin/env bash
# Formatting check for the mod's own code (.clang-format). id's code under
# rerelease/ and vendored code under third_party/ are excluded: they keep their
# upstream formatting so diffs against upstream stay readable.
#
#   scripts/check_format.sh          # check
#   scripts/check_format.sh --fix    # reformat in place
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

TOOLS_VENV=${Q2_TOOLS_VENV:-$HOME/.local/share/q2game-tools}
if [[ -z "${CLANG_FORMAT:-}" && -x "$TOOLS_VENV/bin/clang-format" ]]; then
    CLANG_FORMAT="$TOOLS_VENV/bin/clang-format"
fi
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}
if ! command -v "$CLANG_FORMAT" >/dev/null; then
    if [[ -n "${CI:-}" ]]; then
        echo "check_format: FAIL - clang-format not installed"; exit 1
    fi
    echo "check_format: skipped - clang-format not installed (CI runs it)"; exit 0
fi

# Our own code: everything tracked except id's, vendored and generated files.
mapfile -t files < <(git ls-files --cached --others --exclude-standard -- \
    '*.cpp' '*.h' ':!rerelease/**' ':!third_party/**' \
    ':!checks/stock_client_contract.cpp' ':!tools/print_stock_contract.cpp')
(( ${#files[@]} )) || { echo "check_format: ok (no files)"; exit 0; }

if [[ "${1:-}" == "--fix" ]]; then
    "$CLANG_FORMAT" -i "${files[@]}"; echo "check_format: reformatted ${#files[@]} file(s)"
else
    "$CLANG_FORMAT" --dry-run --Werror "${files[@]}"
    echo "check_format: ok (${#files[@]} file(s))"
fi
