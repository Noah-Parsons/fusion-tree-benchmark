// fusion_node_compact.hpp — a fusion node holding only what a query touches.
//
// FusionNode<8> is 144 bytes: it carries the sorted important-bit positions
// (32 bytes) and three packed constants that a query needs, plus bookkeeping.
// With PEXT the positions are never read at query time: the mask is enough.
// And the broadcast and sentinel constants depend only on (r, n), so they can
// come from one small table shared by every node.
//
// What is left is the keys (64 bytes), the packed sketches and the PEXT mask
// (8 bytes each), and two counts: 88 bytes. With nine child indices a tree
// node is 125 bytes, padded to 128 when aligned to 64 — exactly the size of a
// 64-byte-aligned B-tree node. That is the point: FusionTreeT<Compact, 8, 64>
// against BTree<8, true, 64> compares the two searches at EQUAL memory
// footprint, so any remaining gap is not memory traffic.
//
// The rank is branch-free (branchfree_rank in fusion_node.hpp). The sketch
// always uses sketch_pext, which is one PEXT instruction wherever BMI2 is
// available, whatever FT_USE_PEXT says.
#pragma once
#include "fusion_node.hpp"
#include <cstdint>

namespace ft {

struct FieldTables {
    u64 broadcast[8];      // [r]    : a 1 in the low bit of each of 8 fields of width r+1
    u64 sentinels[8][9];   // [r][n] : a 1 in the sentinel bit of the first n fields
};

constexpr FieldTables make_field_tables() {
    FieldTables t{};
    for (int r = 0; r < 8; ++r) {
        const int f = r + 1;
        for (int i = 0; i < 8; ++i) t.broadcast[r] |= 1ull << (i * f);
        for (int n = 0; n <= 8; ++n)
            for (int i = 0; i < n; ++i) t.sentinels[r][n] |= 1ull << (i * f + r);
    }
    return t;
}

inline constexpr FieldTables kFieldTables = make_field_tables();

class FusionNodeCompact {
public:
    static constexpr int K = 8;

    void build(const u64* sorted_keys, int n) {
        assert(n >= 0 && n <= K);
        n_ = (std::uint8_t)n;
        for (int i = 0; i < K; ++i) keys_[i] = i < n ? sorted_keys[i] : 0;
        // Important bits go straight into the mask; PEXT keeps them in
        // ascending order, so no sorted position list is needed.
        mask_ = 0;
        for (int i = 0; i + 1 < n; ++i) mask_ |= 1ull << highest_diff_bit(keys_[i], keys_[i + 1]);
        r_ = (std::uint8_t)popcount(mask_);
        const int f = r_ + 1;
        packed_ = 0;
        for (int i = 0; i < n; ++i) packed_ |= sketch(keys_[i]) << (i * f);
    }

    int size() const { return n_; }
    u64 key(int i) const { return keys_[i]; }
    int important_bit_count() const { return r_; }
    u64 sketch(u64 x) const { return sketch_pext(x, mask_); }

    int rank_le(u64 s, u64 minus) const {
        u64 query = ((s | (1ull << r_)) - minus) * kFieldTables.broadcast[r_];
        return popcount((query - packed_) & kFieldTables.sentinels[r_][n_]);
    }

    int rank(u64 q) const {
        if (n_ == 0) return 0;
        return branchfree_rank(q, keys_, n_,
            [this](u64 x) { return sketch(x); },
            [this](u64 s, u64 minus) { return rank_le(s, minus); });
    }

    // Every piece of node memory rank(q) reads, as f(address, bytes). The
    // shared tables are 576 bytes in total and are not reported.
    template <typename F>
    void touch(u64 q, F&& f) const {
        f(&n_, sizeof n_);
        f(&r_, sizeof r_);
        f(&mask_, sizeof mask_);
        f(&packed_, sizeof packed_);
        if (n_ == 0) return;
        int i = rank_le(sketch(q), 0);
        f(&keys_[i > 0 ? i - 1 : 0], sizeof(u64));
        f(&keys_[i < n_ ? i : n_ - 1], sizeof(u64));
    }

private:
    u64 keys_[K] = {};
    u64 packed_ = 0;
    u64 mask_ = 0;
    std::uint8_t n_ = 0, r_ = 0;
};

} // namespace ft
