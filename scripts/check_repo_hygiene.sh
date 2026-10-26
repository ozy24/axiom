#!/usr/bin/env bash
# Enforces AGENTS.md hard rule 7 and keeps game data out of git.
#
#  * repos/ holds local reference checkouts (Q2REX is private) and must never
#    be committed.
#  * Retail game data (paks, kpf) and built binaries must never be committed:
#    the repo will be public at 1.0, and retail data can't be redistributed.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

fail=0
tracked=$(git ls-files --cached)

if grep -q '^repos/' <<<"$tracked"; then
    echo "check_repo_hygiene: FAIL - files under repos/ are tracked:"
    grep '^repos/' <<<"$tracked" | head -20 | sed 's/^/  /'
    fail=1
fi

bad=$(grep -iE '\.(pak|kpf|pk3|dll|so|exe|pdb|dmp)$' <<<"$tracked" || true)
if [[ -n "$bad" ]]; then
    echo "check_repo_hygiene: FAIL - game data or binaries are tracked:"
    sed 's/^/  /' <<<"$bad"
    fail=1
fi

# Anything over 2 MB is almost certainly data, not source.
while IFS= read -r f; do
    [[ -f "$f" ]] || continue
    size=$(stat -c %s "$f")
    if (( size > 2*1024*1024 )); then
        echo "check_repo_hygiene: FAIL - $f is $((size/1024)) KB; data does not belong in git"
        fail=1
    fi
done <<<"$tracked"

(( fail == 0 )) && echo "check_repo_hygiene: ok"
exit $fail
