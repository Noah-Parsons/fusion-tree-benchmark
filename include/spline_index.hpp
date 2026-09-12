// spline_index.hpp — a challenger to the S+ tree that does not depend on the
// keys being spread evenly.
//
// RadixJump (radix_jump.hpp) guesses where q belongs from q's top bits. That
// guess is good only when the keys are spread evenly; on clumped keys it falls
// apart. SplineIndex learns the shape of the keys instead.
//
// BUILD. Plot every key against its position in the sorted array. Replace that
// curve with straight pieces (a "spline"), chosen so that no key is more than
// E positions away from its piece. Clumps simply become steep pieces and the
// gaps between them flat ones; the E bound holds whatever the shape. The
// pieces are fitted in one pass by the greedy spline corridor (Neumann and
// Michel, 2008), the fitting method RadixSpline also uses (Kipf et al., 2020).
//
// QUERY.
//   1. FIND THE PIECE. An S+ tree over the pieces' first keys. There are far
//      fewer pieces than keys, so this small tree lives mostly in cache.
//   2. PREDICT. Follow the piece's line to q: one multiply.
//   3. SCAN. The answer is within about E of the prediction, so compare q with
//      the 2E + 4 keys around it at once (AVX2) and count.
//
// An S+ tree over all n keys pays a cache miss on each of its bottom levels.
// This pays for one scan window.
#pragma once
#include "bits.hpp"
#include "splus_tree.hpp"
#include <immintrin.h>
#include <cstddef>
#include <vector>

namespace ft {

template <int E = 16>
class SplineIndex {
    static constexpr u64 FLIP = 0x8000000000000000ull;
    static constexpr int W = 2 * E + 4;            // scan width, a multiple of 4
public:
    SplineIndex() = default;
    SplineIndex(const SplineIndex&) = delete;
    SplineIndex& operator=(const SplineIndex&) = delete;
    ~SplineIndex() { if (keys_) _mm_free(keys_); }

    void build(const std::vector<u64>& a) {
        if (keys_) { _mm_free(keys_); keys_ = nullptr; }
        n_ = a.size();
        keys_ = static_cast<u64*>(_mm_malloc((n_ + W) * sizeof(u64), 64));
        for (std::size_t i = 0; i < n_; ++i) keys_[i] = a[i] ^ FLIP;
        for (std::size_t i = n_; i < n_ + W; ++i) keys_[i] = ~0ull ^ FLIP;   // never < q

        std::vector<u64> xs;
        std::vector<double> ys;
        fit(a, xs, ys);
        seg_.assign(xs.size(), Seg{});
        for (std::size_t s = 0; s < xs.size(); ++s) {
            seg_[s].y = ys[s];
            seg_[s].slope = (s + 1 < xs.size() && xs[s + 1] > xs[s])
                ? (ys[s + 1] - ys[s]) / (double)(xs[s + 1] - xs[s]) : 0.0;
        }
        top_.build(xs);
        if (n_) { xmin_ = a.front(); xmax_ = a.back(); }
    }

    bool predecessor(u64 q, u64& out) const {
        if (n_ == 0) return false;
        // Predict with q held inside the key range; scan with the real q.
        u64 qc = q < xmin_ ? xmin_ : (q > xmax_ ? xmax_ : q);
        // 1. The piece: the last one starting below qc (or the first).
        std::size_t s = top_.lower_bound_index(qc);
        s = s ? s - 1 : 0;
        // 2. Predict.
        const Seg& g = seg_[s];
        long long guess = (long long)(g.y + g.slope * (double)(qc - top_.key_at(s)));
        // The true position is within [guess - E, guess + E + 1], plus one
        // position of slack for rounding on each side.
        long long lo = guess - E - 1;
        lo = lo < 0 ? 0 : lo;
        lo = lo > (long long)n_ ? (long long)n_ : lo;
        // 3. Scan the window.
        const __m256i qv = _mm256_set1_epi64x((long long)(q ^ FLIP));
        // Counted per 4-key chunk: at E = 32 the window is 68 keys, more than
        // one 64-bit mask can hold.
        std::size_t lb = (std::size_t)lo;
        for (int i = 0; i < W; i += 4) {
            __m256i k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys_ + lo + i));
            lb += (std::size_t)popcount((u64)(unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))));
        }
        if (lb == 0) return false;
        out = keys_[lb - 1] ^ FLIP;
        return true;
    }

    std::size_t size() const { return n_; }
    std::size_t pieces() const { return seg_.size(); }
    int levels(u64) const { return 1; }

private:
    struct Seg { double y, slope; };

    // Greedy spline corridor. From the current base point, keep the range of
    // slopes [lower, upper] for which a line from the base passes within E of
    // every key seen so far. When the next key's own slope leaves that range,
    // the previous key becomes a spline point and the new base.
    static void fit(const std::vector<u64>& a, std::vector<u64>& xs, std::vector<double>& ys) {
        const std::size_t n = a.size();
        if (n == 0) return;
        xs.push_back(a[0]); ys.push_back(0.0);
        if (n == 1) return;
        std::size_t base = 0;
        auto slope = [&](std::size_t i, double dy) {
            return ((double)i + dy - (double)base) / (double)(a[i] - a[base]);
        };
        double upper = slope(1, +E), lower = slope(1, -E);
        for (std::size_t i = 2; i < n; ++i) {
            double si = slope(i, 0.0);
            if (si > upper || si < lower) {
                base = i - 1;
                xs.push_back(a[base]); ys.push_back((double)base);
                upper = slope(i, +E);
                lower = slope(i, -E);
            } else {
                double u = slope(i, +E), l = slope(i, -E);
                if (u < upper) upper = u;
                if (l > lower) lower = l;
            }
        }
        xs.push_back(a[n - 1]); ys.push_back((double)(n - 1));
    }

    u64* keys_ = nullptr;
    std::vector<Seg> seg_;
    SPlusTree<16> top_;
    std::size_t n_ = 0;
    u64 xmin_ = 0, xmax_ = 0;
};

using Spline8  = SplineIndex<8>;
using Spline16 = SplineIndex<16>;
using Spline32 = SplineIndex<32>;

} // namespace ft
