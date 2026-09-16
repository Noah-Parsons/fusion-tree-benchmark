// sosd_rivals.hpp — Hist-Tree (compact, CHT) wrapped for the race. RMI is
// added once its code is generated.
//
// CHT (Crotty, CIDR 2021; implementation github.com/stoianmihail/CHT, MIT):
// every node is a histogram of 2^k equal-width bins over its key range; a bin
// with more than max_error keys gets its own child node, as deep as needed.
// The compact layout flattens the tree into one table of 32-bit entries. A
// lookup follows the table until a leaf, which holds the position of the
// first key in that bin; the answer lies within max_error keys of it.
//
// Fairness: finished with the same last step as every other structure here
// (detail::last_mile, from learned_indexes.hpp).
#pragma once
#include "learned_indexes.hpp"
#include <functional>   // CHT's builder uses std::function and std::cerr
#include <iostream>     // without including these itself
#include <optional>
#include <queue>

#ifndef NDEBUG
#define FT_RESTORE_ASSERT_SOSD
#define NDEBUG
#endif
#include <cassert>
#include "cht/builder.h"
#include "cht/cht.h"
#ifdef FT_RESTORE_ASSERT_SOSD
#undef NDEBUG
#undef FT_RESTORE_ASSERT_SOSD
#include <cassert>
#endif

namespace ft {

template <int Bins, int MaxError>
class CHTIndex {
public:
    CHTIndex() = default;
    CHTIndex(const CHTIndex&) = delete;
    CHTIndex& operator=(const CHTIndex&) = delete;
    ~CHTIndex() { if (keys_) _mm_free(keys_); }

    void build(const std::vector<u64>& a) {
        if (keys_) _mm_free(keys_);
        keys_ = detail::flipped_copy(a);
        n_ = a.size();
        // CHT needs a key range at least as wide as its bin count (its first
        // shift is log2(range) - log2(bins), unsigned). Narrower inputs, which
        // occur only in the tiny test cases, are scanned directly instead.
        small_ = n_ < 2 || (a.back() - a.front()) < (u64)Bins;
        if (!small_) {
            cht::Builder<u64> b(a.front(), a.back(), Bins, MaxError, /*single_pass=*/false, /*use_cache=*/false);
            for (u64 k : a) b.AddKey(k);
            cht_ = b.Finalize();
        }
    }

    bool predecessor(u64 q, u64& out) const {
        std::size_t lo = 0, len = n_;
        if (!small_) {
            cht::SearchBound sb = cht_.GetSearchBound(q);
            lo = sb.begin;
            len = sb.end - sb.begin;
        }
        std::size_t lb = detail::last_mile(keys_, lo, len, q);
        if (lb == 0) return false;
        out = keys_[lb - 1] ^ detail::LI_FLIP;
        return true;
    }

    std::size_t size() const { return n_; }
    std::size_t bytes() const { return (n_ + 8) * sizeof(u64) + (small_ ? 0 : cht_.GetSize()); }
    int levels(u64) const { return 1; }

private:
    u64* keys_ = nullptr;
    std::size_t n_ = 0;
    bool small_ = true;
    cht::CompactHistTree<u64> cht_;
};

// The settings raced: bins in {64, 256, 1024} × max error in {16, 32, 64, 128}.
using CHT64E16   = CHTIndex<64, 16>;
using CHT64E32   = CHTIndex<64, 32>;
using CHT64E64   = CHTIndex<64, 64>;
using CHT64E128  = CHTIndex<64, 128>;
using CHT256E16  = CHTIndex<256, 16>;
using CHT256E32  = CHTIndex<256, 32>;
using CHT256E64  = CHTIndex<256, 64>;
using CHT256E128 = CHTIndex<256, 128>;
using CHT1024E16  = CHTIndex<1024, 16>;
using CHT1024E32  = CHTIndex<1024, 32>;
using CHT1024E64  = CHTIndex<1024, 64>;
using CHT1024E128 = CHTIndex<1024, 128>;

} // namespace ft
