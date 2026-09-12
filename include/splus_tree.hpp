// splus_tree.hpp — the S+ tree, the strongest known static search tree.
//
// Sergey Slotin's S+ tree (Algorithmica, "Static B-Trees", 2021) is a B+ tree
// with every trick for modern hardware applied at once:
//
//   * NO POINTERS. Node k's children are nodes k*(B+1) .. k*(B+1)+B on the
//     layer below, so a child's address is computed, never loaded.
//   * ALL KEYS IN THE LEAVES. Upper layers only hold copies of the smallest
//     key of each right-hand subtree, so a descent never stops early and has
//     a fixed number of steps.
//   * SIMD NODE SEARCH. All B keys of a node are compared with the query at
//     once and the "less than" results counted. No branches.
//   * PRE-FLIPPED KEYS. AVX2 compares signed numbers; keys are stored with
//     their top bit flipped, so only the query is flipped, once per search.
//   * ONE CONTIGUOUS, 64-BYTE-ALIGNED ALLOCATION.
//
// This version is adapted from the original's 32-bit keys to the 64-bit keys
// of this study and from lower_bound to predecessor (largest key < q). Slotin
// also uses AVX-512 and 2 MB huge pages; this machine has no AVX-512, and
// huge pages are not used by any structure here, so the comparison is fair.
#pragma once
#include "bits.hpp"
#include <immintrin.h>
#include <cstddef>
#include <vector>

namespace ft {

template <int B = 16>
class SPlusTree {
    static constexpr u64 FLIP = 0x8000000000000000ull;
    static constexpr u64 PAD  = ~0ull ^ FLIP;   // flipped UINT64_MAX: never < q
public:
    SPlusTree() = default;
    SPlusTree(const SPlusTree&) = delete;
    SPlusTree& operator=(const SPlusTree&) = delete;
    ~SPlusTree() { if (mem_) _mm_free(mem_); }

    void build(const std::vector<u64>& a) {
        if (mem_) { _mm_free(mem_); mem_ = nullptr; }
        n_ = a.size();
        // Layer 0 is the leaves. n/B + 1 blocks, so there is always at least
        // one padding slot and a lower_bound of n has somewhere to land.
        std::size_t nodes[16];
        H_ = 0;
        nodes[H_++] = n_ / B + 1;
        while (nodes[H_ - 1] > 1) {
            nodes[H_] = (nodes[H_ - 1] + B) / (B + 1);
            ++H_;
        }
        std::size_t total = 0;
        for (int h = 0; h < H_; ++h) total += nodes[h] * B;
        mem_ = static_cast<u64*>(_mm_malloc(total * sizeof(u64), 64));
        // Root layer first in memory, leaves last.
        std::size_t off = 0;
        for (int h = H_ - 1; h >= 0; --h) { layer_[h] = mem_ + off; off += nodes[h] * B; }

        for (std::size_t i = 0; i < nodes[0] * B; ++i)
            layer_[0][i] = i < n_ ? a[i] ^ FLIP : PAD;
        // Separator j of node k on layer h is the smallest key of child j+1:
        // the first key of that child's leftmost leaf.
        u64 span = 1;                                   // (B+1)^(h-1)
        for (int h = 1; h < H_; ++h) {
            for (std::size_t k = 0; k < nodes[h]; ++k)
                for (int j = 0; j < B; ++j) {
                    u64 leaf = (u64(k) * (B + 1) + j + 1) * span;
                    u64 idx = leaf * B;
                    layer_[h][k * B + j] = (leaf < nodes[0] && idx < n_) ? a[idx] ^ FLIP : PAD;
                }
            span *= (B + 1);
        }
    }

    // Position of the first key >= q, or n if there is none.
    std::size_t lower_bound_index(u64 q) const {
        const __m256i qv = _mm256_set1_epi64x((long long)(q ^ FLIP));
        std::size_t k = 0;
        for (int h = H_ - 1; h > 0; --h)
            k = k * (B + 1) + rank(qv, layer_[h] + k * B);
        return k * B + rank(qv, layer_[0] + k * B);
    }

    bool predecessor(u64 q, u64& out) const {
        std::size_t lb = lower_bound_index(q);
        if (lb == 0) return false;
        out = layer_[0][lb - 1] ^ FLIP;
        return true;
    }

    u64 key_at(std::size_t i) const { return layer_[0][i] ^ FLIP; }
    std::size_t size() const { return n_; }
    int levels(u64) const { return H_; }

private:
    // How many of the node's B keys are less than q. Four keys per compare.
    static unsigned rank(__m256i qv, const u64* p) {
        unsigned m = 0;
        for (int i = 0; i < B; i += 4) {
            __m256i k = _mm256_load_si256(reinterpret_cast<const __m256i*>(p + i));
            m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))) << i;
        }
        return (unsigned)popcount(m);
    }

    u64* mem_ = nullptr;
    u64* layer_[16] = {};
    std::size_t n_ = 0;
    int H_ = 0;
};

using SPlus8  = SPlusTree<8>;    // splus8:  one cache line per node
using SPlus16 = SPlusTree<16>;   // splus16: the original's 16 keys per node

} // namespace ft
