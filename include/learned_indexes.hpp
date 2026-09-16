// learned_indexes.hpp — the two best-known published learned indexes, wrapped
// so they race under the same rules as everything else.
//
// A learned index does not search a tree. It learns the shape of the keys
// (how position grows with key value) and uses that to guess where q belongs,
// with a promise that the true place is within a small distance of the guess.
//
//   RadixSpline (Kipf et al., 2020): a straight-line spline with error bound E,
//     found through a radix table on the top R bits of the key.
//   PGM-index (Ferragina and Vinciguerra, 2020): straight-line pieces with
//     error bound Epsilon, themselves indexed by smaller PGM levels.
//
// FAIRNESS. Both libraries only return a range [lo, hi) holding the answer;
// their examples finish with std::lower_bound. Here they finish with exactly
// the last step ClusterJump uses (branch-free halving to at most 8 keys, then
// one AVX2 compare), so no structure wins or loses on its final step.
#pragma once
#include "bits.hpp"
#include <immintrin.h>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

// Their headers use assert(), some of it inside the search. Compile them the
// way their authors benchmark them, without asserts, then restore assert.
#ifndef NDEBUG
#define FT_RESTORE_ASSERT
#define NDEBUG
#endif
#include <cassert>
#include "rs/builder.h"
#include "rs/radix_spline.h"
#include "pgm/pgm_index.hpp"
#ifdef FT_RESTORE_ASSERT
#undef NDEBUG
#undef FT_RESTORE_ASSERT
#include <cassert>
#endif

namespace ft {
namespace detail {

static constexpr u64 LI_FLIP = 0x8000000000000000ull;

// Keys stored with the top bit flipped, followed by 8 padding slots.
inline u64* flipped_copy(const std::vector<u64>& a) {
    u64* k = static_cast<u64*>(_mm_malloc((a.size() + 8) * sizeof(u64), 64));
    if (!k) throw std::bad_alloc();   // fail cleanly under a memory cap
    for (std::size_t i = 0; i < a.size(); ++i) k[i] = a[i] ^ LI_FLIP;
    for (std::size_t i = a.size(); i < a.size() + 8; ++i) k[i] = ~0ull ^ LI_FLIP;
    return k;
}

// The first position >= q, given that it lies in [lo, lo + len]. The same code
// as ClusterJump's steps 2 and 3.
inline std::size_t last_mile(const u64* keys, std::size_t lo, std::size_t len, u64 q) {
    const long long qs = (long long)(q ^ LI_FLIP);
    while (len > 8) {
        std::size_t half = len / 2;
        lo += ((long long)keys[lo + half] < qs) ? (len - half) : 0;
        len = half;
    }
    const __m256i qv = _mm256_set1_epi64x(qs);
    unsigned m = 0;
    for (int i = 0; i < 8; i += 4) {
        __m256i k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys + lo + i));
        m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))) << i;
    }
    return lo + (std::size_t)popcount(m & ((1u << len) - 1));
}

} // namespace detail

// RadixSpline with R radix bits and error bound E.
template <int R, int E>
class RSIndex {
public:
    RSIndex() = default;
    RSIndex(const RSIndex&) = delete;
    RSIndex& operator=(const RSIndex&) = delete;
    ~RSIndex() { if (keys_) _mm_free(keys_); }

    void build(const std::vector<u64>& a) {
        if (keys_) _mm_free(keys_);
        keys_ = detail::flipped_copy(a);
        n_ = a.size();
        // RadixSpline needs at least two distinct keys (it takes the leading
        // zeros of max - min, undefined for 0). Tiny inputs are just scanned.
        small_ = n_ < 2;
        if (!small_) {
            rs::Builder<u64> b(a.front(), a.back(), R, E);
            for (u64 k : a) b.AddKey(k);
            rs_ = b.Finalize();
        }
    }

    bool predecessor(u64 q, u64& out) const {
        std::size_t lo = 0, len = n_;
        if (!small_) {
            rs::SearchBound sb = rs_.GetSearchBound(q);
            lo = sb.begin;
            len = sb.end - sb.begin;
        }
        std::size_t lb = detail::last_mile(keys_, lo, len, q);
        if (lb == 0) return false;
        out = keys_[lb - 1] ^ detail::LI_FLIP;
        return true;
    }

    std::size_t size() const { return n_; }
    std::size_t bytes() const { return (n_ + 8) * sizeof(u64) + (small_ ? 0 : rs_.GetSize()); }
    int levels(u64) const { return 1; }

private:
    u64* keys_ = nullptr;
    std::size_t n_ = 0;
    bool small_ = true;
    rs::RadixSpline<u64> rs_;
};

// PGM-index with error bound Eps (upper levels use the library default, 4).
template <int Eps>
class PGMWrap {
public:
    PGMWrap() = default;
    PGMWrap(const PGMWrap&) = delete;
    PGMWrap& operator=(const PGMWrap&) = delete;
    ~PGMWrap() { if (keys_) _mm_free(keys_); }

    void build(const std::vector<u64>& a) {
        if (keys_) _mm_free(keys_);
        keys_ = detail::flipped_copy(a);
        n_ = a.size();
        // The PGM-index reserves UINT64_MAX as a sentinel and refuses it as a
        // key. If it is present it can only be the last key, and it is never
        // < q, so indexing the other keys gives the same answers.
        m_ = (n_ && a.back() == ~0ull) ? n_ - 1 : n_;
        pgm_ = m_ ? pgm::PGMIndex<u64, Eps>(a.begin(), a.begin() + m_) : pgm::PGMIndex<u64, Eps>();
    }

    bool predecessor(u64 q, u64& out) const {
        std::size_t lo = 0, len = m_;
        if (m_) {
            // Asking for the sentinel itself would walk past the index, so
            // UINT64_MAX is looked up as UINT64_MAX - 1, with one extra slot.
            pgm::ApproxPos r = pgm_.search(q == ~0ull ? q - 1 : q);
            std::size_t hi = r.hi + 1 < m_ ? r.hi + 1 : m_;
            lo = r.lo;
            len = hi - r.lo;
        }
        std::size_t lb = detail::last_mile(keys_, lo, len, q);
        if (lb == 0) return false;
        out = keys_[lb - 1] ^ detail::LI_FLIP;
        return true;
    }

    std::size_t size() const { return n_; }
    std::size_t bytes() const { return (n_ + 8) * sizeof(u64) + (m_ ? pgm_.size_in_bytes() : 0); }
    int levels(u64) const { return 1; }

private:
    u64* keys_ = nullptr;
    std::size_t n_ = 0, m_ = 0;
    pgm::PGMIndex<u64, Eps> pgm_;
};

// The configurations raced. RadixSpline: the authors' default is R = 18,
// E = 32; the SOSD benchmark sweeps both. PGM-index: the authors' default is
// Epsilon = 64; SOSD sweeps powers of two.
using RS18E8  = RSIndex<18, 8>;
using RS18E16 = RSIndex<18, 16>;
using RS18E32 = RSIndex<18, 32>;
using RS22E8  = RSIndex<22, 8>;
using RS22E16 = RSIndex<22, 16>;
using RS22E32 = RSIndex<22, 32>;
using PGM16   = PGMWrap<16>;
using PGM32   = PGMWrap<32>;
using PGM64   = PGMWrap<64>;
using PGM128  = PGMWrap<128>;

} // namespace ft
