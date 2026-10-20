// cluster_jump2.hpp — ClusterJump with wider slots and outlier-robust second levels.
//
// Same structure as cluster_jump.hpp (read that first): an equal-width top table, then for each
// top bucket a second table fitted to the bucket's keys, then a short search over the keys.
// Two changes, described in results/best/DESIGN_cj2.md:
//
//   K  The second table targets 2^K keys per slot (ClusterJump uses K = 2). Fewer slots make the
//      second tables small enough to stay in cache.
//   W  The last step compares W keys at once (8 or 16) with AVX2. Longer ranges are halved first.
//   Robust fitting. If a bucket's 8 largest (or smallest) keys stretch its range more than 4x,
//      the second table is fitted to the remaining keys; the stretched ones are reached through
//      the end slot (or slot 0), which holds at most 8 of them.
//
// Depth is fixed (top table, second table, keys); there is no recursion and no equal-width
// splitting of crowded slots.
#pragma once
#include "bits.hpp"
#include <immintrin.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ft {

template <int K, int W, bool Robust = true>
class ClusterJump2 {
    static_assert(W == 8 || W == 16, "W must be 8 or 16");
    static constexpr u64 FLIP = 0x8000000000000000ull;
    static constexpr std::size_t R = 8;          // keys an end slot may take over
public:
    ClusterJump2() = default;
    ClusterJump2(const ClusterJump2&) = delete;
    ClusterJump2& operator=(const ClusterJump2&) = delete;
    ~ClusterJump2() { if (keys_) _mm_free(keys_); }

    void build(const std::vector<u64>& a) {
        if (keys_) { _mm_free(keys_); keys_ = nullptr; }
        n_ = a.size();
        keys_ = static_cast<u64*>(_mm_malloc((n_ + W) * sizeof(u64), 64));
        for (std::size_t i = 0; i < n_; ++i) keys_[i] = a[i] ^ FLIP;
        for (std::size_t i = n_; i < n_ + W; ++i) keys_[i] = ~0ull ^ FLIP;
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
            else i1 = n_;
            Top& t = top_[b];
            t.start = (std::uint32_t)t2_.size();
            std::size_t k = i1 - i0;
            if (k == 0) {
                t.base = 0; t.shift = 63; t.slots = 0;
                t2_.push_back((std::uint32_t)i0);
                t2_.push_back((std::uint32_t)i0);
            } else {
                // Fit range [lo, hi] (indices); keys outside it go to the end slots.
                std::size_t lo = i0, hi = i1 - 1;
                if (Robust && k > 4 * R) {
                    if (a[hi] - a[lo] > 4 * (a[hi - R] - a[lo])) hi -= R;
                    if (a[hi] - a[lo] > 4 * (a[hi] - a[lo + R])) lo += R;
                }
                t.base = a[lo];
                std::size_t kf = hi - lo + 1;
                int lgs = (kf >> K) ? 63 - __builtin_clzll(kf >> K) : 0;
                t.shift = (std::uint32_t)shift_for(a[hi] - t.base, lgs);
                t.slots = (std::uint32_t)(((a[hi] - t.base) >> t.shift) + 1);
                // Slot s holds every key of the bucket with (key - base) >> shift == s, including
                // left-out keys that happen to fall inside the fitted range; slot 0 also holds the
                // keys below base, and slot `slots` only the keys whose slot number is >= slots
                // (at most R of them). (Fixed 2026-10-19: the first version put all R left-out
                // keys in the end slot, losing those that fall inside the last regular slot.)
                std::size_t j = lo;
                t2_.push_back((std::uint32_t)i0);
                for (u64 s = 1; s <= t.slots; ++s) {
                    while (j < i1 && ((a[j] - t.base) >> t.shift) < s) ++j;
                    t2_.push_back((std::uint32_t)j);
                }
                t2_.push_back((std::uint32_t)i1);
            }
            i0 = i1;
        }
    }

    bool predecessor(u64 q, u64& out) const {
        u64 tb = (q - gbase_) >> gshift_;
        tb = q < gbase_ ? 0 : tb;
        tb = tb < ntop_ ? tb : ntop_;
        const Top& t = top_[tb];
        u64 j = (q - t.base) >> t.shift;
        j = q < t.base ? 0 : j;
        j = j < t.slots ? j : t.slots;
        std::size_t lo = t2_[t.start + j];
        std::size_t len = t2_[t.start + j + 1] - lo;
        const long long qs = (long long)(q ^ FLIP);
        while (len > (std::size_t)W) {
            std::size_t half = len / 2;
            lo += ((long long)keys_[lo + half] < qs) ? (len - half) : 0;
            len = half;
        }
        const __m256i qv = _mm256_set1_epi64x(qs);
        unsigned m = 0;
        for (int i = 0; i < W; i += 4) {
            __m256i kv = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys_ + lo + i));
            m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, kv))) << i;
        }
        std::size_t lb = lo + (std::size_t)popcount(m & ((1u << len) - 1));
        if (lb == 0) return false;
        out = keys_[lb - 1] ^ FLIP;
        return true;
    }

    std::size_t size() const { return n_; }
    std::size_t bytes() const {
        return (n_ + W) * sizeof(u64) + top_.size() * sizeof(Top) + t2_.size() * sizeof(std::uint32_t);
    }
    int levels(u64) const { return 2; }

private:
    struct Top { u64 base; std::uint32_t start, slots, shift; };
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

using CJ2_K2W8  = ClusterJump2<2, 8>;
using CJ2_K3W16 = ClusterJump2<3, 16>;
using CJ2_K4W16 = ClusterJump2<4, 16>;

} // namespace ft
