# Known bugs in id's code

Bugs in the stock rerelease game code that Axiom fixes on purpose. A fix makes
Axiom differ from the stock reference, so each one needs:

1. an entry here: what the bug is, where it was found (MuffMode, Q2RED, our own
   testing), and the commit that fixes it;
2. a test that reproduces it;
3. if it changes saved state, a line in `docs/baselines/allowed-differences.txt`
   pointing back here.

Without all three, the comparison tests report the fix as a regression.

_None ported yet._
