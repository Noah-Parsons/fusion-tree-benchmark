// sketch_fast.hpp — two ways to make sketching O(1) instead of O(r).
//
// The naive sketch in bits.hpp loops once per important bit. The theory needs
// it done in constant time. There are two routes.
//
// ROUTE A — the 1993 route: multiplication.
//   Fredman and Willard show that for any set of r important bit positions
//   there exists a multiplier M such that multiplying by M shifts all r bits
//   into a window of width at most r^4, in order and without collisions. The
//   multiplier is found by a greedy search. This is the construction that
//   gives the fusion tree its theoretical constant factor, and measuring how
//   wide that window actually comes out is one of the experiments in this
//   project.
//
// ROUTE B — the 2013 route: ask the hardware.
//   Intel's BMI2 extension has PEXT, which extracts the bits of a word at the
//   positions given by a mask and packs them down to the bottom — that is
//   precisely the sketch operation, as one instruction. It did not exist when
//   the fusion tree was designed. On Intel and on AMD Zen 3 and later it runs
//   in about 3 cycles; on AMD Zen 1 and Zen 2 it is microcoded and takes
//   roughly 18, which is itself worth measuring if you have access to both.
//
// Compile anything including this with -mbmi2 to enable Route B.
#pragma once
#include "bits.hpp"
#include <vector>

#if defined(__BMI2__)
#include <immintrin.h>
#endif

namespace ft {

// ---- Route B: one instruction ---------------------------------------------
// mask has a 1 at each important bit position.
inline u64 sketch_pext(u64 x, u64 mask) {
#if defined(__BMI2__)
    return _pext_u64(x, mask);
#else
    // Portable fallback, so the code still builds without BMI2.
    u64 s = 0; int out = 0;
    for (int b = 0; b < W; ++b)
        if ((mask >> b) & 1ull) { s |= ((x >> b) & 1ull) << out; ++out; }
    return s;
#endif
}

inline u64 mask_from_positions(const int* pos, int r) {
    u64 m = 0;
    for (int i = 0; i < r; ++i) m |= 1ull << pos[i];
    return m;
}

// ---- Route A: the multiplier search ---------------------------------------
struct Multiplier {
    u64 M = 0;            // the multiplier itself
    std::vector<int> m;   // per-bit shifts, m[i] pairs with positions[i]
    int lo = 0;           // lowest landing position, positions[i] + m[i]
    int hi = 0;           // highest landing position
    int spread() const { return hi - lo + 1; }
    bool ok = false;      // did a valid multiplier fit inside a 64-bit word?
};

// Greedy construction, following Fredman & Willard (1993), Lemma 3.
//
// We need shifts m_0..m_{r-1} such that every cross term positions[i] + m[j]
// is distinct. Distinctness is what guarantees that when the masked key is
// multiplied by M = sum of 2^m[j], no two bits land on the same position, so
// nothing carries and nothing is destroyed.
//
// Chosen greedily: take the smallest shift that does not collide with
// anything already placed. A candidate's own cross terms pos[i] + cand
// cannot collide with each other, because the positions are distinct, so
// only collisions with earlier landings need checking. The landings are
// kept in a bitset, so each candidate costs r lookups; with a list it cost
// r times the number of landings, which is too slow at r = 15.
inline Multiplier find_multiplier(const int* pos, int r) {
    Multiplier res;
    if (r == 0) { res.ok = true; return res; }
    constexpr int LIMIT = 4096;
    std::vector<bool> used(LIMIT + W, false);   // every landing position so far
    res.m.assign(r, 0);
    for (int t = 0; t < r; ++t) {
        for (int cand = 0; cand < LIMIT; ++cand) {
            bool clash = false;
            for (int i = 0; i < r && !clash; ++i) clash = used[pos[i] + cand];
            if (clash) continue;
            res.m[t] = cand;
            for (int i = 0; i < r; ++i) used[pos[i] + cand] = true;
            break;
        }
    }
    res.lo = res.hi = pos[0] + res.m[0];
    for (int i = 0; i < r; ++i) {
        int p = pos[i] + res.m[i];
        if (p < res.lo) res.lo = p;
        if (p > res.hi) res.hi = p;
    }
    res.M = 0;
    for (int i = 0; i < r; ++i) res.M |= 1ull << res.m[i];
    res.ok = (res.hi < W);
    return res;
}

} // namespace ft
