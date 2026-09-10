// bits.hpp — the three low-level primitives a fusion node is built from.
//
// Everything here operates on a single 64-bit machine word. Nothing here
// allocates, branches on data, or touches memory beyond registers.
#pragma once
#include <cstdint>
#include <cassert>

namespace ft {

using u64 = std::uint64_t;

// W is the word width in bits. Every bound in this project is stated in terms
// of it, so it is named rather than written as a literal 64.
constexpr int W = 64;

// ---------------------------------------------------------------------------
// Primitive 1: most significant set bit.
//
// msb(x) returns the index of the highest set bit of x, counting from 0 at the
// least significant end. msb(1) == 0, msb(8) == 3, msb(~0ull) == 63.
// Undefined for x == 0 — callers must check.
//
// The theory demands this in O(1). On x86-64 it is a single LZCNT or BSR
// instruction, so it genuinely is. __builtin_clzll counts LEADING ZEROS, so
// the index of the top bit is 63 - clz.
// ---------------------------------------------------------------------------
inline int msb(u64 x) {
    assert(x != 0);
    return 63 - __builtin_clzll(x);
}

// The index of the highest bit at which a and b differ.
// This is the length of their common prefix, counted from the top.
// Undefined when a == b.
inline int highest_diff_bit(u64 a, u64 b) {
    assert(a != b);
    return msb(a ^ b);
}

// ---------------------------------------------------------------------------
// Primitive 2: population count.
//
// Counts set bits. One POPCNT instruction on any x86-64 chip made since 2008.
// Used to turn the result of a parallel comparison into a rank.
// ---------------------------------------------------------------------------
inline int popcount(u64 x) {
    return __builtin_popcountll(x);
}

// ---------------------------------------------------------------------------
// Primitive 3: bit extraction at a fixed set of positions.
//
// Given a key x and a list of bit positions, produce the word formed by
// concatenating the bits of x at those positions, lowest position at the
// lowest output bit.
//
// This is the NAIVE sketch: it costs one iteration per position, so it is
// O(r), not O(1). It is correct, it is easy to check, and it is the version
// you build first. The O(1) versions (PEXT, and the multiplier search) live
// in sketch_fast.hpp; `make test-loop` runs the tests against this one.
// ---------------------------------------------------------------------------
inline u64 extract_bits_naive(u64 x, const int* positions, int r) {
    u64 s = 0;
    for (int i = 0; i < r; ++i) {
        s |= ((x >> positions[i]) & 1ull) << i;
    }
    return s;
}

} // namespace ft
