# Third-party code used in the learned-index race

Downloaded 2026-09-12. Shallow clones; the `.git`
folders record the exact commit.

| library | source | commit | license |
|---|---|---|---|
| RadixSpline | https://github.com/learnedsystems/RadixSpline | ab96aa59d429e7423beba2350bdcdf88952df282 (2021-05-13) | MIT |
| PGM-index | https://github.com/gvinciguerra/PGM-index | c6fcf3d34e55eb0061b01e2f49dfcbdb711f1407 (2024-11-28) | Apache-2.0 |
| ALEX | https://github.com/microsoft/ALEX | 4370da6aa8b509fdc9b0d2c49faa0624b0078589 (2024-03-12) | MIT |
| TLX | https://github.com/tlx/tlx | 2dd63ab839909f0b43cd02108a1869c4670b2c8e (2025-01-10) | BSL-1.0 |

ALEX and TLX were downloaded later the same day, for the changing-data race.
Cite: Ding et al., "ALEX: an updatable adaptive learned index", SIGMOD 2020;
Bingmann, "TLX: collection of sophisticated C++ data structures", 2018.

Cite: Kipf et al., "RadixSpline: a single-pass learned index", aiDM 2020;
Ferragina and Vinciguerra, "The PGM-index", PVLDB 13(8), 2020.

## Changes made

**RadixSpline, `include/rs/builder.h`, one line.** `__builtin_clzl(diff)` became
`__builtin_clzll(diff)`. On Windows a `long` is 32 bits, so the original counts
leading zeros of only the low half of a 64-bit key range and picks the wrong
radix shift. On Linux (where it was written) the two are identical.

**ALEX: no source changes.** It is compiled as C++17 (it uses
`std::allocator::rebind`, removed in C++20). Its wrapper
(`include/dynamic_rivals.hpp`) answers queries below the smallest or above
the largest stored key itself, because ALEX gives wrong answers for them:
`int bucketID = this->model_.predict(key)` in `alex_nodes.h` overflows for
keys far outside the model's range. Two further ALEX limits were found by
`tests/test_dynamic.cpp` and are not worked around: wrong answers for keys
above 2^53 spaced closer than double precision when the index starts empty,
and a `std::bad_array_new_length` crash on tight, far-apart clumps inserted
into an empty index.

**TLX: no changes.** Default B+ tree settings.

**No other changes.** Both libraries are included from
`include/learned_indexes.hpp`, which compiles them with `NDEBUG` defined so
their `assert` checks (some of them inside the search) are off, as in their
authors' own benchmarks.
