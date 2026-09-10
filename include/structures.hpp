// structures.hpp — the three things being compared.
//
//   FusionTree<K>  the structure under test: a static multiway search tree
//                  whose nodes are fusion nodes
//   SortedArray    baseline 1: branchless binary search over a sorted array
//   BTree<B>       baseline 2: a static B-tree with cache-line-sized nodes
//
// All three answer the same query: predecessor(q) = the largest stored key
// strictly less than q. All three are STATIC — built once from a sorted array,
// never updated. That is deliberate. Insertion and deletion would add a large
// amount of code and would not change the question being asked.
#pragma once
#include "fusion_node.hpp"
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
private:
    std::vector<u64> a_;
};

// ---------------------------------------------------------------------------
// Baseline 2: static B-tree with B keys per node.
//
// B = 8 gives 64 bytes of keys. The whole node (keys + child indices + count)
// is 112 bytes, so a node spans two cache lines, not one. The fusion node is
// 184 bytes (about three lines): the two trees share a shape but not a
// memory footprint.
//
// Branchless = false: the within-node scan stops at the first key >= q. That
//   is one data-dependent branch per key examined, which random queries
//   mispredict.
// Branchless = true:  every slot is compared and the results summed. Fixed
//   trip count, no data-dependent branch. This is the stronger baseline.
// ---------------------------------------------------------------------------
template <int B = 8, bool Branchless = false>
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
            int r = 0;
            if constexpr (Branchless) {
                for (int i = 0; i < B; ++i) r += (i < nd.n) & (nd.key[i] < q);
            } else {
                while (r < nd.n && nd.key[r] < q) ++r;   // early-exit scan
            }
            if (r > 0) { out = nd.key[r - 1]; found = true; }
            idx = nd.leaf ? -1 : nd.child[r];
        }
        return found;
    }

private:
    struct Node {
        u64 key[B];
        int child[B + 1];
        int n;
        bool leaf;
    };

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
// Structurally identical to the B-tree above; the only difference is that the
// within-node search is a fusion node's O(1) rank instead of a scan. That is
// the whole experiment: same shape, same memory layout, one operation swapped.
// Any difference in measured time is attributable to the node search.
// ---------------------------------------------------------------------------
template <int K = 8>
class FusionTree {
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

private:
    struct Node {
        FusionNode<K> fn;
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
            std::vector<u64> ks(s.begin() + lo, s.begin() + hi);
            nd.fn.build(ks.data(), (int)ks.size());
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

} // namespace ft
