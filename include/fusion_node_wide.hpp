// fusion_node_wide.hpp — Experiment 4: a fusion node on a 256-bit word.
//
// The branching factor is limited by K * K <= w. On a 64-bit word that gives
// K = 8. An AVX2 register is w = 256 bits, which allows K = 16: sixteen
// fields of sixteen bits. A 16-key node has at most 15 important bits, so
// each sketch fits in 15 bits and bit 15 of each field is the sentinel,
// exactly as in the 64-bit node.
//
// One difference should be stated plainly in the report. AVX2 subtracts the
// sixteen 16-bit lanes independently, so the hardware itself stops borrows
// at field boundaries. The sentinel still carries the answer, but it no
// longer has to protect the neighbouring field. On a 256-bit word the
// parallel comparison and an ordinary SIMD comparison are the same thing.
//
// Sketches use PEXT on the 64-bit keys. The rank is branch-free
// (branchfree_rank in fusion_node.hpp). Without AVX2 a scalar loop stands in,
// so the header still builds anywhere.
#pragma once
#include "fusion_node.hpp"
#include <cstdint>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace ft {

class FusionNodeWide {
public:
    static constexpr int K = 16;

    void build(const u64* sorted_keys, int n) {
        assert(n >= 0 && n <= K);
        n_ = (std::uint8_t)n;
        for (int i = 0; i < K; ++i) keys_[i] = i < n ? sorted_keys[i] : 0;
        mask_ = 0;
        for (int i = 0; i + 1 < n; ++i) mask_ |= 1ull << highest_diff_bit(keys_[i], keys_[i + 1]);
        r_ = (std::uint8_t)popcount(mask_);
        for (int i = 0; i < K; ++i) packed_[i] = i < n ? (std::uint16_t)sketch(keys_[i]) : 0;
    }

    int size() const { return n_; }
    u64 key(int i) const { return keys_[i]; }
    int important_bit_count() const { return r_; }
    u64 sketch(u64 x) const { return sketch_pext(x, mask_); }

    // Count of live fields i with ((s | 2^15) - minus - sketch_i) >= 2^15:
    // sketch_i <= s when minus = 0, sketch_i < s when minus = 1.
    int lanes_rank(u64 s, u64 minus) const {
        const std::uint16_t qv = (std::uint16_t)((s | 0x8000u) - minus);
#if defined(__AVX2__)
        const __m256i iota = _mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7,
                                               8, 9, 10, 11, 12, 13, 14, 15);
        __m256i q    = _mm256_set1_epi16((short)qv);
        __m256i p    = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(packed_));
        __m256i d    = _mm256_sub_epi16(q, p);
        __m256i live = _mm256_cmpgt_epi16(_mm256_set1_epi16((short)n_), iota);
        unsigned m   = (unsigned)_mm256_movemask_epi8(_mm256_and_si256(d, live));
        return popcount(m & 0xAAAAAAAAu);    // bit 15 of each lane = odd mask bits
#else
        int c = 0;
        for (int i = 0; i < n_; ++i) c += ((std::uint16_t)(qv - packed_[i]) >> 15) & 1;
        return c;
#endif
    }

    int rank(u64 q) const {
        if (n_ == 0) return 0;
        return branchfree_rank(q, keys_, n_,
            [this](u64 x) { return sketch(x); },
            [this](u64 s, u64 minus) { return lanes_rank(s, minus); });
    }

    // Every piece of node memory rank(q) reads, as f(address, bytes).
    template <typename F>
    void touch(u64 q, F&& f) const {
        f(&n_, sizeof n_);
        f(&mask_, sizeof mask_);
        f(packed_, sizeof packed_);
        if (n_ == 0) return;
        int i = lanes_rank(sketch(q), 0);
        f(&keys_[i > 0 ? i - 1 : 0], sizeof(u64));
        f(&keys_[i < n_ ? i : n_ - 1], sizeof(u64));
    }

private:
    alignas(32) std::uint16_t packed_[K] = {};
    u64 keys_[K] = {};
    u64 mask_ = 0;
    std::uint8_t n_ = 0, r_ = 0;
};

} // namespace ft
