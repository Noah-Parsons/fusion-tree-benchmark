// radix_jump.hpp — a challenger to the S+ tree.
//
// Idea. Every tree, the S+ tree included, spends its first several steps
// finding out roughly where q is: "somewhere in the top half", "somewhere in
// that quarter", and so on. Each step is a trip to memory. But the top bits of
// q already say roughly where it is. So skip the tree:
//
//   1. JUMP. Take the top bits of (q - smallest key) as an index into a table.
//      Entry b holds the position of the first key whose top bits are >= b.
//      Entries b and b+1 bracket every key sharing q's top bits: q's answer
//      lies between them. One memory read replaces the upper tree.
//   2. NARROW. If that bracket holds more than 8 keys, halve it
//      (branch-free binary search) until it holds 8 or fewer.
//   3. SCAN. Compare q with those 8 keys at once (AVX2) and count.
//
// The table has about n/4 entries, so buckets hold about 4 keys on average
// when the keys are spread evenly, and step 2 almost never runs.
//
// The weakness is built in. If the keys are clustered, most table entries are
// empty and a few buckets hold huge runs of keys, and step 2 becomes a plain
// binary search. That case is measured too (bench_pred's `clustered` keys).
#pragma once
#include "bits.hpp"
#include <immintrin.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ft {

class RadixJump {
    static constexpr u64 FLIP = 0x8000000000000000ull;
public:
    RadixJump() = default;
    RadixJump(const RadixJump&) = delete;
    RadixJump& operator=(const RadixJump&) = delete;
    ~RadixJump() { if (keys_) _mm_free(keys_); }

    void build(const std::vector<u64>& a) {
        if (keys_) { _mm_free(keys_); keys_ = nullptr; }
        n_ = a.size();
        keys_ = static_cast<u64*>(_mm_malloc((n_ + 8) * sizeof(u64), 64));
        for (std::size_t i = 0; i < n_; ++i) keys_[i] = a[i] ^ FLIP;
        for (std::size_t i = n_; i < n_ + 8; ++i) keys_[i] = ~0ull ^ FLIP;   // padding, never read as a key
        table_.clear();
        if (n_ == 0) { base_ = 0; shift_ = 63; buckets_ = 0; table_.assign(2, 0); return; }

        base_ = a[0];
        u64 span = a[n_ - 1] - base_;
        int want = 1;                                   // log2(number of buckets)
        while ((std::size_t(1) << (want + 2)) < n_ && want < 30) ++want;
        int width = span ? 64 - __builtin_clzll(span) : 0;
        shift_ = width > want ? width - want : 0;
        buckets_ = (span >> shift_) + 1;
        table_.resize(buckets_ + 2);
        std::size_t j = 0;
        for (u64 b = 0; b < buckets_; ++b) {
            while (j < n_ && ((a[j] - base_) >> shift_) < b) ++j;
            table_[b] = (std::uint32_t)j;
        }
        table_[buckets_] = table_[buckets_ + 1] = (std::uint32_t)n_;
    }

    bool predecessor(u64 q, u64& out) const {
        // 1. Jump. Below the smallest key -> bucket 0; beyond the largest -> the
        //    empty bucket past the end.
        u64 b = (q - base_) >> shift_;
        b = q < base_ ? 0 : b;
        b = b < buckets_ ? b : buckets_;
        std::size_t lo = table_[b];
        std::size_t len = table_[b + 1] - lo;
        const long long qs = (long long)(q ^ FLIP);
        // 2. Narrow. The answer's position stays within [lo, lo + len].
        while (len > 8) {
            std::size_t half = len / 2;
            lo += ((long long)keys_[lo + half] < qs) ? (len - half) : 0;
            len = half;
        }
        // 3. Scan the last <= 8 keys at once.
        const __m256i qv = _mm256_set1_epi64x(qs);
        unsigned m = 0;
        for (int i = 0; i < 8; i += 4) {
            __m256i k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys_ + lo + i));
            m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))) << i;
        }
        std::size_t lb = lo + (std::size_t)popcount(m & ((1u << len) - 1));
        if (lb == 0) return false;
        out = keys_[lb - 1] ^ FLIP;
        return true;
    }

    std::size_t size() const { return n_; }
    int levels(u64) const { return 1; }

private:
    u64* keys_ = nullptr;
    std::vector<std::uint32_t> table_;
    std::size_t n_ = 0;
    u64 base_ = 0, buckets_ = 0;
    int shift_ = 0;
};

} // namespace ft
