// fusion_node.hpp — a single fusion tree node.
//
// A fusion node stores up to K sorted 64-bit keys and answers
//
//     rank(q) = the number of stored keys strictly less than q
//
// in O(1) word operations, independent of K. Predecessor and successor follow
// immediately from the rank.
//
// The whole point of the structure is that the comparison step does not loop
// over the keys. All K keys are compared against the query simultaneously,
// inside one machine word, using one multiplication, one subtraction, one AND
// and one POPCNT.
//
// WHY K IS AT MOST 8 HERE
// -----------------------
// Each key is reduced to a "sketch" of r bits, where r is the number of
// important bit positions and r <= K-1. Sketches are packed into a single word
// with one spare sentinel bit each, so each field is r+1 <= K bits wide, and K
// fields must fit in W = 64 bits:
//
//     K * K <= 64   =>   K <= 8
//
// That bound is not a limitation of this implementation. It is the structure
// telling you something, and it is a large part of what this project measures.
#pragma once
#include "bits.hpp"
#include "sketch_fast.hpp"
#include <algorithm>
#include <array>

namespace ft {

// ---------------------------------------------------------------------------
// The branch-free rank, shared by every branch-free node type.
//
// Same algorithm as FusionNode::rank below, rearranged so that no branch
// depends on the query:
//
//   * the two neighbouring candidates are clamped into range instead of
//     skipped;
//   * the candidate sharing the longer prefix with q is the one whose XOR
//     with q is smaller (a lower highest set bit means a smaller value);
//   * both repair values are computed and one is selected;
//   * the "<=" and "<" comparisons become ONE parallel comparison: taking 1
//     off the query field turns "count sketches <= s" into "count sketches
//     < s". When s = 0 the field becomes 2^r - 1, whose sentinel never
//     survives, so the count is correctly 0.
//
// `sk(x)` computes a sketch. `le(s, minus)` counts stored sketches <= s when
// minus = 0 and < s when minus = 1, in one parallel comparison.
// Requires n >= 1 (tree nodes are never empty).
// ---------------------------------------------------------------------------
template <typename Sketch, typename RankLE>
inline int branchfree_rank(u64 q, const u64* keys, int n, Sketch sk, RankLE le) {
    int i  = le(sk(q), 0);
    int lo = i > 0 ? i - 1 : 0;
    int hi = i < n ? i : n - 1;
    u64 xa = q ^ keys[lo], xb = q ^ keys[hi];
    u64 x  = xa < xb ? xa : xb;
    int exact = xa == 0 ? lo : hi;          // used only when x == 0
    int h  = msb(x | 1);                    // | 1 keeps msb defined at x == 0
    u64 bit   = (q >> h) & 1ull;
    u64 below = (1ull << h) - 1;
    u64 e = bit ? ((q & ~(1ull << h)) | below)    // sibling block below q: its max
                : ((q | (1ull << h)) & ~below);   // sibling block above q: its min
    int rk = le(sk(e), bit ^ 1ull);
    return x == 0 ? exact : rk;
}

// BranchFree = false: the rank as written below, with early returns and a
// data-dependent choice between the two repair cases.
// BranchFree = true:  branchfree_rank above. Same answers, no branch that
// depends on the query.
template <int K = 8, bool BranchFree = false>
class FusionNode {
    static_assert(K >= 1 && K <= 8, "K*K must fit in a 64-bit word");

public:
    FusionNode() : n_(0), r_(0), field_(1), packed_(0), broadcast_(0), sentinels_(0) {}

    // Build the node from a sorted, duplicate-free array of at most K keys.
    // O(K^2) — this is a construction cost, paid once, not a query cost.
    void build(const u64* sorted_keys, int n) {
        assert(n >= 0 && n <= K);
        n_ = n;
        for (int i = 0; i < n; ++i) keys_[i] = sorted_keys[i];

        // ---- Step 1: find the important bit positions ------------------
        // An important bit is a position at which two adjacent keys first
        // differ, reading from the top. Equivalently: the depths of the
        // branching nodes in the binary trie of the keys. There are at most
        // n-1 of them, because a trie over n leaves has n-1 branching nodes.
        r_ = 0;
        for (int i = 0; i + 1 < n; ++i) {
            int b = highest_diff_bit(keys_[i], keys_[i + 1]);
            bool seen = false;
            for (int j = 0; j < r_; ++j) if (imp_[j] == b) { seen = true; break; }
            if (!seen) imp_[r_++] = b;
        }
        std::sort(imp_.begin(), imp_.begin() + r_);   // ascending
        mask_ = mask_from_positions(imp_.data(), r_);

        // ---- Step 2: pack the sketches ---------------------------------
        // Field layout, for field width f = r+1:
        //
        //   bit index:  ... [ i*f + r ] [ i*f + r-1 ... i*f ] ...
        //                    sentinel        sketch of key i
        //
        // The sentinel bit of every stored field is 0. In the query word it
        // is 1. That is what makes the subtraction trick work.
        field_ = r_ + 1;
        packed_ = 0;
        broadcast_ = 0;
        sentinels_ = 0;
        for (int i = 0; i < n; ++i) {
            packed_ |= sketch(keys_[i]) << (i * field_);
            broadcast_ |= 1ull << (i * field_);
            sentinels_ |= 1ull << (i * field_ + r_);
        }
    }

    int size() const { return n_; }
    u64 key(int i) const { return keys_[i]; }

    // The sketch of a key: its bits at the important positions, concatenated.
    // O(r) with the loop; O(1) with PEXT. See sketch_fast.hpp.
    u64 sketch(u64 x) const {
#ifdef FT_USE_PEXT
        return sketch_pext(x, mask_);      // one instruction, O(1)
#else
        return extract_bits_naive(x, imp_.data(), r_);   // O(r) loop
#endif
    }

    // ---- The parallel comparison -----------------------------------------
    // Returns the number of stored sketches that are <= s.
    //
    // Broadcasting s (with its sentinel bit set) into every field and
    // subtracting the packed sketches leaves field i holding
    //
    //     2^r + s - sketch_i
    //
    // which is >= 2^r exactly when s >= sketch_i. Since every sketch is
    // strictly below 2^r, no field can borrow from its neighbour, so the
    // fields stay independent. The surviving sentinel bits are then counted.
    int sketch_rank_le(u64 s) const {
        if (n_ == 0) return 0;
        u64 query = ((s | (1ull << r_)) * broadcast_);
        u64 diff  = query - packed_;
        return popcount(diff & sentinels_);
    }

    // Number of stored sketches strictly less than s.
    int sketch_rank_lt(u64 s) const {
        if (s == 0) return 0;
        return sketch_rank_le(s - 1);
    }

    // ---- rank ------------------------------------------------------------
    // Returns the number of stored keys strictly less than q.
    //
    // Sketching throws information away, so a sketch comparison alone can put
    // the query in the wrong place. The repair is the "desketchify" step: find
    // the stored key sharing the longest prefix with q, look at the first bit
    // where they differ, and replace q with a value that has the same rank but
    // whose sketch is guaranteed faithful.
    int rank(u64 q) const {
        if (n_ == 0) return 0;
        if constexpr (BranchFree) {
            return branchfree_rank(q, keys_.data(), n_,
                [this](u64 x) { return sketch(x); },
                [this](u64 s, u64 minus) {
                    u64 query = ((s | (1ull << r_)) - minus) * broadcast_;
                    return popcount((query - packed_) & sentinels_);
                });
        }

        // Provisional position from the sketch alone.
        int i = sketch_rank_le(sketch(q));

        // The two neighbouring stored keys are the only candidates for
        // longest common prefix with q.
        int best = -1;
        int best_h = W;                       // lower h == longer shared prefix
        for (int c = i - 1; c <= i; ++c) {
            if (c < 0 || c >= n_) continue;
            if (keys_[c] == q) return c;      // exact hit: rank is its index
            int h = highest_diff_bit(q, keys_[c]);
            if (h < best_h) { best_h = h; best = c; }
        }
        if (best < 0) return 0;

        const int h = best_h;

        // Every stored key that shares q's prefix above bit h must carry the
        // OPPOSITE bit at h — otherwise it would share a longer prefix with q
        // than the best match does, which is impossible by construction.
        // Those keys form one contiguous block, the "sibling subtree".
        //
        // The repair value e is the far edge of that sibling subtree. Two
        // facts make e safe to sketch, and they are the reason this works
        // where sketching q directly does not:
        //
        //   * against a key INSIDE the block, e agrees above h and dominates
        //     (or is dominated) bitwise below h, so the sketch comparison
        //     lands on the correct side;
        //   * against a key OUTSIDE the block, e first differs at some bit p
        //     above h — and p is the branching bit between two stored keys,
        //     so p IS an important bit and the sketch sees it.
        if ((q >> h) & 1ull) {
            // q branches HIGH at h, so the whole sibling subtree is below q.
            // Take its maximum: bit h cleared, everything below set.
            u64 e = (q & ~(1ull << h)) | ((1ull << h) - 1);
            return sketch_rank_le(sketch(e));
        } else {
            // q branches LOW at h, so the whole sibling subtree is above q.
            // Take its minimum: bit h set, everything below cleared.
            u64 e = (q | (1ull << h)) & ~((1ull << h) - 1);
            return sketch_rank_lt(sketch(e));
        }
    }

    // Largest stored key strictly less than q, or false if there is none.
    bool predecessor(u64 q, u64& out) const {
        int rk = rank(q);
        if (rk == 0) return false;
        out = keys_[rk - 1];
        return true;
    }

    int important_bit_count() const { return r_; }
    const std::array<int, K>& important_bits() const { return imp_; }

    // Report every piece of node memory rank(q) reads, as f(address, bytes).
    // Used only by the traffic experiment (bench/traffic.cpp).
    template <typename F>
    void touch(u64 q, F&& f) const {
        f(&n_, sizeof n_);
        f(&r_, sizeof r_);
#ifdef FT_USE_PEXT
        f(&mask_, sizeof mask_);
#else
        f(imp_.data(), sizeof(int) * r_);
#endif
        f(&broadcast_, sizeof broadcast_);
        f(&packed_, sizeof packed_);
        f(&sentinels_, sizeof sentinels_);
        if (n_ == 0) return;
        int i = sketch_rank_le(sketch(q));
        f(&keys_[i > 0 ? i - 1 : 0], sizeof(u64));
        f(&keys_[i < n_ ? i : n_ - 1], sizeof(u64));
    }

private:
    int n_;
    int r_;
    int field_;
    u64 packed_;
    u64 broadcast_;
    u64 sentinels_;
    std::array<u64, K> keys_{};
    std::array<int, K> imp_{};
    u64 mask_ = 0;
};

} // namespace ft
