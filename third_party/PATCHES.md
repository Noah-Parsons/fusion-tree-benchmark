# Third-party code used in the learned-index race

Downloaded 2026-09-12 with the user's permission. Shallow clones; the `.git`
folders record the exact commit.

| library | source | commit | license |
|---|---|---|---|
| RadixSpline | https://github.com/learnedsystems/RadixSpline | ab96aa59d429e7423beba2350bdcdf88952df282 (2021-05-13) | MIT |
| PGM-index | https://github.com/gvinciguerra/PGM-index | c6fcf3d34e55eb0061b01e2f49dfcbdb711f1407 (2024-11-28) | Apache-2.0 |

Cite: Kipf et al., "RadixSpline: a single-pass learned index", aiDM 2020;
Ferragina and Vinciguerra, "The PGM-index", PVLDB 13(8), 2020.

## Changes made

**RadixSpline, `include/rs/builder.h`, one line.** `__builtin_clzl(diff)` became
`__builtin_clzll(diff)`. On Windows a `long` is 32 bits, so the original counts
leading zeros of only the low half of a 64-bit key range and picks the wrong
radix shift. On Linux (where it was written) the two are identical.

**No other changes.** Both libraries are included from
`include/learned_indexes.hpp`, which compiles them with `NDEBUG` defined so
their `assert` checks (some of them inside the search) are off, as in their
authors' own benchmarks.
