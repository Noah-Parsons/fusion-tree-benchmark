// cluster_jump.hpp — RadixJump that survives clustered keys.
//
// RadixJump (radix_jump.hpp) is fast because a search is only a few
// instructions: one table read, one short scan. It fails on clustered keys
// because one table stretched over the whole 64-bit range leaves almost every
// entry empty and puts whole clusters into single entries.
//
// ClusterJump keeps the short path and adds one level:
//
//   1. TOP. q's top bits pick one of at most 2^16 top buckets. This table is
//      about 1.5 MB, so it sits in cache.
//   2. RESCALE. Each top bucket stores where its own keys start and how far
//      they spread, and has its own second table sized for about 4 keys per
//      entry across exactly that spread. A cluster that lands in one top
//      bucket gets a fine table of its own; an empty stretch of the key range
//      gets an empty bucket that costs nothing.
//   3. SCAN. The second-table entry brackets q's answer; narrow to at most 8
//      keys and compare them at once (AVX2).
//
// What this does NOT promise. It adapts to clustering at one scale: clusters
// that are themselves made of tight sub-clusters would again crowd single
// entries, and step 3 would fall back to binary search. SplineIndex
// (spline_index.hpp) has a hard error bound for any shape, at the price of a
// longer search.
#pragma once
#include "bits.hpp"
#include <immintrin.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ft {

class ClusterJump {
    static constexpr u64 FLIP = 0x8000000000000000ull;
public:
    ClusterJump() = default;
    ClusterJump(const ClusterJump&) = delete;
    ClusterJump& operator=(const ClusterJump&) = delete;
    ~ClusterJump() { if (keys_) _mm_free(keys_); }

    void build(const std::vector<u64>& a) {
        if (keys_) { _mm_free(keys_); keys_ = nullptr; }
        n_ = a.size();
        keys_ = static_cast<u64*>(_mm_malloc((n_ + 8) * sizeof(u64), 64));
        for (std::size_t i = 0; i < n_; ++i) keys_[i] = a[i] ^ FLIP;
        for (std::size_t i = n_; i < n_ + 8; ++i) keys_[i] = ~0ull ^ FLIP;
        top_.clear();
        t2_.clear();

        if (n_ == 0) {
            gbase_ = 0; gshift_ = 63; ntop_ = 0;
            top_.push_back(Top{0, 0, 0, 63});
            t2_.assign(2, 0);
            return;
        }
        gbase_ = a[0];
        int lgn = 63 - __builtin_clzll(n_);
        int tbits = lgn - 6 < 1 ? 1 : (lgn - 6 > 16 ? 16 : lgn - 6);
        gshift_ = shift_for(a[n_ - 1] - gbase_, tbits);
        ntop_ = ((a[n_ - 1] - gbase_) >> gshift_) + 1;
        top_.resize(ntop_ + 1);

        std::size_t i0 = 0;
        for (u64 b = 0; b <= ntop_; ++b) {
            std::size_t i1 = i0;
            if (b < ntop_) while (i1 < n_ && ((a[i1] - gbase_) >> gshift_) == b) ++i1;
            else i1 = n_;                           // the sentinel past the end
            Top& t = top_[b];
            t.start = (std::uint32_t)t2_.size();
            std::size_t k = i1 - i0;
            if (k == 0) {
                // Empty bucket: every q here has the same answer position.
                t.base = 0; t.shift = 63; t.slots = 0;
                t2_.push_back((std::uint32_t)i0);
                t2_.push_back((std::uint32_t)i0);
            } else {
                t.base = a[i0];
                int lgs = (k >> 2) ? 63 - __builtin_clzll(k >> 2) : 0;   // ~4 keys per slot
                t.shift = (std::uint32_t)shift_for(a[i1 - 1] - t.base, lgs);
                t.slots = (std::uint32_t)(((a[i1 - 1] - t.base) >> t.shift) + 1);
                std::size_t j = i0;
                for (u64 s = 0; s < t.slots; ++s) {
                    while (j < i1 && ((a[j] - t.base) >> t.shift) < s) ++j;
                    t2_.push_back((std::uint32_t)j);
                }
                t2_.push_back((std::uint32_t)i1);
                t2_.push_back((std::uint32_t)i1);
            }
            i0 = i1;
        }
    }

    bool predecessor(u64 q, u64& out) const {
        // 1. Top bucket (q below every key -> 0; beyond -> the sentinel).
        u64 tb = (q - gbase_) >> gshift_;
        tb = q < gbase_ ? 0 : tb;
        tb = tb < ntop_ ? tb : ntop_;
        const Top& t = top_[tb];
        // 2. Slot in that bucket's own table.
        u64 j = (q - t.base) >> t.shift;
        j = q < t.base ? 0 : j;
        j = j < t.slots ? j : t.slots;
        std::size_t lo = t2_[t.start + j];
        std::size_t len = t2_[t.start + j + 1] - lo;
        // 3. Narrow to at most 8 keys, then compare them at once.
        const long long qs = (long long)(q ^ FLIP);
        while (len > 8) {
            std::size_t half = len / 2;
            lo += ((long long)keys_[lo + half] < qs) ? (len - half) : 0;
            len = half;
        }
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
    int levels(u64) const { return 2; }

private:
    struct Top { u64 base; std::uint32_t start, slots, shift; };

    // The shift that cuts a spread of `span` into about 2^bits pieces.
    static int shift_for(u64 span, int bits) {
        int width = span ? 64 - __builtin_clzll(span) : 0;
        return width > bits ? width - bits : 0;
    }

    u64* keys_ = nullptr;
    std::vector<Top> top_;
    std::vector<std::uint32_t> t2_;
    std::size_t n_ = 0;
    u64 gbase_ = 0, ntop_ = 0;
    int gshift_ = 0;
};

} // namespace ft
