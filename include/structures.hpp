// structures.hpp — the things being compared.
//
//   FusionTreeT<Node, K, Align>  the structure under test: a static multiway
//                  search tree whose nodes are fusion nodes of arity K
//   FusionTree<K>  the original: FusionNode<K> nodes, natural alignment
//   SortedArray    baseline 1: branchless binary search over a sorted array
//   BTree<B, Branchless, Align>  baseline 2: a static B-tree
//
// All answer the same query: predecessor(q) = the largest stored key
// strictly less than q. All are STATIC — built once from a sorted array,
// never updated. That is deliberate. Insertion and deletion would add a large
// amount of code and would not change the question being asked.
#pragma once
#include "fusion_node.hpp"
#include "fusion_node_compact.hpp"
#include "fusion_node_wide.hpp"
#include <vector>
#include <cstddef>

namespace ft {

// ---------------------------------------------------------------------------
// Baseline 1: sorted array + binary search.
//
// This is the thing to beat. It is three lines long, it has perfect spatial
// locality at the end of the search and terrible locality at the start, and
// on any modern machine it is very hard to improve on for small n.
// ---------------------------------------------------------------------------
class SortedArray {
public:
    void build(const std::vector<u64>& sorted) { a_ = sorted; }

    bool predecessor(u64 q, u64& out) const {
        std::size_t lo = 0, len = a_.size();
        // Branchless lower_bound: no data-dependent branch, so no
        // mispredictions. Uses conditional moves instead.
        while (len > 0) {
            std::size_t half = len / 2;
            lo += (a_[lo + half] < q) ? (len - half) : 0;
            len = half;
        }
        if (lo == 0) return false;
        out = a_[lo - 1];
        return true;
    }
    std::size_t size() const { return a_.size(); }
    int levels(u64) const { return 0; }

    // Every piece of memory predecessor(q) reads, as f(address, bytes).
    template <typename F>
    void touch(u64 q, F&& f) const {
        std::size_t lo = 0, len = a_.size();
        while (len > 0) {
            std::size_t half = len / 2;
            f(&a_[lo + half], sizeof(u64));
            lo += (a_[lo + half] < q) ? (len - half) : 0;
            len = half;
        }
        if (lo > 0) f(&a_[lo - 1], sizeof(u64));
    }
private:
    std::vector<u64> a_;
};

// ---------------------------------------------------------------------------
// Baseline 2: static B-tree with B keys per node.
//
// B = 8 gives 64 bytes of keys. With child indices and a count the node is
// 112 bytes: two cache lines, not one. Align = 64 pads it to 128 bytes and
// starts every node on a line boundary — the same footprint as a tree node
// built from FusionNodeCompact.
//
// Branchless = false: the within-node scan stops at the first key >= q. That
//   is one data-dependent branch per key examined, which random queries
//   mispredict.
// Branchless = true:  every slot is compared and the results summed. Fixed
//   trip count, no data-dependent branch.
// ---------------------------------------------------------------------------
template <int B = 8, bool Branchless = false, int Align = alignof(u64)>
class BTree {
public:
    void build(const std::vector<u64>& sorted) {
        nodes_.clear();
        if (!sorted.empty()) root_ = build_range(sorted, 0, sorted.size());
        else root_ = -1;
    }

    bool predecessor(u64 q, u64& out) const {
        if (root_ < 0) return false;
        bool found = false;
        int idx = root_;
        while (idx >= 0) {
            const Node& nd = nodes_[idx];
            int r = node_rank(nd, q);
            if (r > 0) { out = nd.key[r - 1]; found = true; }
            idx = nd.leaf ? -1 : nd.child[r];
        }
        return found;
    }

    // Number of nodes a query visits. Diagnostic only.
    int levels(u64 q) const {
        int d = 0;
        for (int idx = root_; idx >= 0; ++d) {
            const Node& nd = nodes_[idx];
            idx = nd.leaf ? -1 : nd.child[node_rank(nd, q)];
        }
        return d;
    }

    static constexpr std::size_t node_bytes() { return sizeof(Node); }

    // Every piece of memory predecessor(q) reads, as f(address, bytes).
    template <typename F>
    void touch(u64 q, F&& f) const {
        for (int idx = root_; idx >= 0;) {
            const Node& nd = nodes_[idx];
            int r = node_rank(nd, q);
            int scanned = Branchless ? B : (r < nd.n ? r + 1 : nd.n);
            f(nd.key, sizeof(u64) * scanned);
            f(&nd.n, sizeof nd.n);
            f(&nd.leaf, sizeof nd.leaf);
            if (r > 0) f(&nd.key[r - 1], sizeof(u64));
            if (!nd.leaf) f(&nd.child[r], sizeof(int));
            idx = nd.leaf ? -1 : nd.child[r];
        }
    }

private:
    struct alignas(Align) Node {
        u64 key[B];
        int child[B + 1];
        int n;
        bool leaf;
    };

    // The branch-free scan is written with explicit AVX2 instructions: four
    // keys per compare, no jumps. Written as a plain loop, g++ 16 vectorised
    // it in one benchmark harness and emitted eight scalar compare/and/add
    // groups in another, from identical source. The scalar version took
    // 443 ns per query at n = 2^25 against 193 ns for the vector one, which
    // would have quietly weakened the baseline (found 10 Sep 2026; see
    // Experiments_Report_2026-09-10.docx). The fusion nodes use explicit
    // instructions (PEXT, POPCNT, AVX2) too, so both sides are now spelled out.
    static int node_rank(const Node& nd, u64 q) {
        int r = 0;
        if constexpr (Branchless) {
#if defined(__AVX2__)
            if constexpr (B % 4 == 0) {
                // AVX2 compares SIGNED 64-bit lanes; flipping the top bit of
                // both sides turns that into the unsigned "key < q" we need.
                const __m256i flip = _mm256_set1_epi64x((long long)0x8000000000000000ull);
                const __m256i qv = _mm256_xor_si256(_mm256_set1_epi64x((long long)q), flip);
                unsigned m = 0;
                for (int i = 0; i < B; i += 4) {
                    __m256i k = _mm256_xor_si256(
                        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&nd.key[i])), flip);
                    __m256i lt = _mm256_cmpgt_epi64(qv, k);
                    m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(lt)) << i;
                }
                return popcount(m & ((1u << nd.n) - 1));   // only the n live keys
            }
#endif
            for (int i = 0; i < B; ++i) r += (i < nd.n) & (nd.key[i] < q);
        } else {
            while (r < nd.n && nd.key[r] < q) ++r;   // early-exit scan
        }
        return r;
    }

    int build_range(const std::vector<u64>& s, std::size_t lo, std::size_t hi) {
        int self = (int)nodes_.size();
        nodes_.push_back(Node{});
        std::size_t count = hi - lo;
        if (count <= (std::size_t)B) {
            Node nd{};
            nd.n = (int)count;
            nd.leaf = true;
            for (std::size_t i = 0; i < count; ++i) nd.key[i] = s[lo + i];
            nodes_[self] = nd;
            return self;
        }
        // Choose B separators splitting the range into B+1 nearly equal parts.
        Node nd{};
        nd.n = B;
        nd.leaf = false;
        std::size_t bounds[B + 2];
        for (int i = 0; i <= B + 1; ++i)
            bounds[i] = lo + (count * i) / (B + 1);
        for (int i = 0; i < B; ++i) {
            std::size_t sep = bounds[i + 1];
            if (sep >= hi) sep = hi - 1;
            nd.key[i] = s[sep];
            bounds[i + 1] = sep;
        }
        nodes_[self] = nd;
        int kids[B + 1];
        std::size_t start = lo;
        for (int i = 0; i <= B; ++i) {
            std::size_t end = (i == B) ? hi : bounds[i + 1];
            kids[i] = (end > start) ? build_range(s, start, end) : -1;
            start = (i == B) ? hi : bounds[i + 1] + 1;
        }
        for (int i = 0; i <= B; ++i) nodes_[self].child[i] = kids[i];
        return self;
    }

    std::vector<Node> nodes_;
    int root_ = -1;
};

// ---------------------------------------------------------------------------
// The fusion tree.
//
// Structurally identical to the B-tree above: same arity, same separators,
// same build, same descent loop. The only difference is that the
// within-node search is a fusion node's O(1) rank instead of a scan. The
// node type is a parameter so that the node's layout, its branches and its
// word width can each be changed on their own:
//
//   FusionTreeT<FusionNode<8>, 8>         the original
//   FusionTreeT<FusionNode<8, true>, 8>   same layout, branch-free rank
//   FusionTreeT<FusionNodeCompact, 8, 64> 128-byte nodes, branch-free rank
//   FusionTreeT<FusionNodeWide, 16, 64>   K = 16 on a 256-bit word
// ---------------------------------------------------------------------------
template <typename NodeT, int K, int Align = alignof(NodeT)>
class FusionTreeT {
public:
    void build(const std::vector<u64>& sorted) {
        nodes_.clear();
        root_ = sorted.empty() ? -1 : build_range(sorted, 0, sorted.size());
    }

    bool predecessor(u64 q, u64& out) const {
        if (root_ < 0) return false;
        bool found = false;
        int idx = root_;
        while (idx >= 0) {
            const Node& nd = nodes_[idx];
            int r = nd.fn.rank(q);
            if (r > 0) { out = nd.fn.key(r - 1); found = true; }
            idx = nd.leaf ? -1 : nd.child[r];
        }
        return found;
    }

    int levels(u64 q) const {
        int d = 0;
        for (int idx = root_; idx >= 0; ++d) {
            const Node& nd = nodes_[idx];
            idx = nd.leaf ? -1 : nd.child[nd.fn.rank(q)];
        }
        return d;
    }

    static constexpr std::size_t node_bytes() { return sizeof(Node); }

    // Every piece of memory predecessor(q) reads, as f(address, bytes).
    template <typename F>
    void touch(u64 q, F&& f) const {
        for (int idx = root_; idx >= 0;) {
            const Node& nd = nodes_[idx];
            nd.fn.touch(q, f);
            int r = nd.fn.rank(q);
            f(&nd.leaf, sizeof nd.leaf);
            if (!nd.leaf) f(&nd.child[r], sizeof(int));
            idx = nd.leaf ? -1 : nd.child[r];
        }
    }

private:
    struct alignas(Align) Node {
        NodeT fn;
        int child[K + 1];
        bool leaf;
    };

    int build_range(const std::vector<u64>& s, std::size_t lo, std::size_t hi) {
        int self = (int)nodes_.size();
        nodes_.push_back(Node{});
        std::size_t count = hi - lo;
        if (count <= (std::size_t)K) {
            Node nd{};
            nd.leaf = true;
            nd.fn.build(s.data() + lo, (int)count);
            nodes_[self] = nd;
            return self;
        }
        std::size_t bounds[K + 2];
        for (int i = 0; i <= K + 1; ++i)
            bounds[i] = lo + (count * i) / (K + 1);
        u64 seps[K];
        for (int i = 0; i < K; ++i) {
            std::size_t sep = bounds[i + 1];
            if (sep >= hi) sep = hi - 1;
            seps[i] = s[sep];
            bounds[i + 1] = sep;
        }
        Node nd{};
        nd.leaf = false;
        nd.fn.build(seps, K);
        nodes_[self] = nd;
        int kids[K + 1];
        std::size_t start = lo;
        for (int i = 0; i <= K; ++i) {
            std::size_t end = (i == K) ? hi : bounds[i + 1];
            kids[i] = (end > start) ? build_range(s, start, end) : -1;
            start = (i == K) ? hi : bounds[i + 1] + 1;
        }
        for (int i = 0; i <= K; ++i) nodes_[self].child[i] = kids[i];
        return self;
    }

    std::vector<Node> nodes_;
    int root_ = -1;
};

template <int K = 8>
using FusionTree = FusionTreeT<FusionNode<K>, K>;

// ---------------------------------------------------------------------------
// Every structure in the study, by the name used in the result files.
// ---------------------------------------------------------------------------
using BTree8          = BTree<8>;                       // btree8
using BTree8BL        = BTree<8, true>;                 // btree8_bl
using BTree8A64       = BTree<8, false, 64>;            // btree8_a64
using BTree8BLA64     = BTree<8, true, 64>;             // btree8_bl_a64
using BTree16BLA64    = BTree<16, true, 64>;            // btree16_bl_a64
using Fusion8         = FusionTree<8>;                  // fusion8
using Fusion8BF       = FusionTreeT<FusionNode<8, true>, 8>;   // fusion8_bf
using Fusion8C        = FusionTreeT<FusionNodeCompact, 8, 64>; // fusion8_c
using Fusion16W       = FusionTreeT<FusionNodeWide, 16, 64>;   // fusion16_w

} // namespace ft
