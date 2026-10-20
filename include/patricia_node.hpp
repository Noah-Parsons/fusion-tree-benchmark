// patricia_node.hpp — one static node of a HOT-style trie (Binna et al.,
// SIGMOD 2018 / TODS 2022), built for predecessor search.
//
// The node holds up to K <= 32 sorted keys. It looks only at their
// DISCRIMINATIVE bits: the bit where each pair of neighbouring keys first
// differs. There are at most K-1 such bits, so PEXT squeezes any key down to
// at most 31 bits (its "sketch"), and 32 sketches fill two cache lines.
//
// Two operations:
//
//   step(q)  The blind Patricia descent: follow q's bits at the
//            discriminative positions only, and return the key it lands on.
//            Done with HOT's "sparse partial keys": entry i stores only the
//            bits on its own root-to-leaf path, and the answer is the last
//            entry whose stored 1-bits are all 1 in q's sketch. One PEXT, one
//            AVX2 test, one bit scan. In a full trie this is the whole cost
//            of a level; the landing key may be wrong for q, but that is
//            corrected ONCE per query, at the bottom.
//
//   rank(q)  The number of keys < q, exactly. step(q) lands on y; the top bit
//            where q and y differ, p, tells which side of a whole block of
//            keys q falls; a second search on the ordinary ("dense") sketches
//            of a key built from q's bits above p then counts them. This is
//            the fusion tree's two-sketch search, with PEXT and AVX2 doing
//            each half.
#pragma once
#include "bits.hpp"
#include <immintrin.h>
#include <cstdint>

namespace ft {

template <int K>
struct PatriciaNode {
    static_assert(K == 16 || K == 32, "K must be 16 or 32");
    alignas(64) std::uint32_t sparse[K];
    alignas(64) std::uint32_t dense[K];
    u64 key[K];
    u64 mask;
    int n;

    void build(const u64* k, int cnt) {
        n = cnt;
        mask = 0;
        for (int i = 0; i + 1 < cnt; ++i) mask |= 1ull << msb(k[i] ^ k[i + 1]);
        u64 path[K] = {};
        split(k, path, 0, cnt);
        for (int i = 0; i < K; ++i) {
            key[i] = i < cnt ? k[i] : ~0ull;
            // Padding: all-ones never complies (a sketch has at most 31 bits)
            // and is above every real sketch.
            sparse[i] = i < cnt ? (std::uint32_t)_pext_u64(path[i], mask) : 0xFFFFFFFFu;
            dense[i]  = i < cnt ? (std::uint32_t)_pext_u64(k[i], mask) : 0xFFFFFFFFu;
        }
    }

    // Index of the key the blind descent lands on.
    int step(u64 q) const {
        const __m256i dq = _mm256_set1_epi32((int)_pext_u64(q, mask));
        unsigned m = 0;
        for (int i = 0; i < K; i += 8) {
            __m256i s = _mm256_load_si256(reinterpret_cast<const __m256i*>(&sparse[i]));
            __m256i ok = _mm256_cmpeq_epi32(_mm256_and_si256(s, dq), s);
            m |= (unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(ok)) << i;
        }
        return 31 - __builtin_clz(m);       // entry 0 always complies, so m != 0
    }

    // Number of keys < q.
    int rank(u64 q) const {
        const int j = step(q);
        const u64 y = key[j];
        const u64 x = q ^ y;
        const unsigned p = x ? 63u - (unsigned)_lzcnt_u64(x) : 0u;
        const u64 low = _bzhi_u64(~0ull, p);          // bits below p
        const bool gt = q > y;
        // q above y: every key sharing q's bits above p is below q, so count
        // keys <= q with its low bits set. q below y: count keys < q with its
        // low bits cleared. (q == y: x = 0, low = 0, and this counts keys < y.)
        const u64 e = gt ? (q | low) : (q & ~low);
        const std::uint32_t t = (std::uint32_t)_pext_u64(e, mask) + (gt ? 1u : 0u);
        const __m256i flip = _mm256_set1_epi32((int)0x80000000u);
        const __m256i tv = _mm256_xor_si256(_mm256_set1_epi32((int)t), flip);
        unsigned m = 0;
        for (int i = 0; i < K; i += 8) {
            __m256i d = _mm256_xor_si256(
                _mm256_load_si256(reinterpret_cast<const __m256i*>(&dense[i])), flip);
            m |= (unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(tv, d))) << i;
        }
        return popcount(m);
    }

private:
    // Mark, for every key, the discriminative bits where its path goes right.
    static void split(const u64* k, u64* path, int lo, int hi) {
        if (hi - lo < 2) return;
        const int d = msb(k[lo] ^ k[hi - 1]);
        int m = lo;
        while (!((k[m] >> d) & 1)) ++m;
        for (int i = m; i < hi; ++i) path[i] |= 1ull << d;
        split(k, path, lo, m);
        split(k, path, m, hi);
    }
};

} // namespace ft
