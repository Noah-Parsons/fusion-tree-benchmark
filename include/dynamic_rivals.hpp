// dynamic_rivals.hpp — published structures that accept inserts, wrapped with
// the same three calls as ClusterJumpD: bulk_load, insert, predecessor.
//
//   TlxSet     the B+ tree of the TLX library (Bingmann), a widely used, fast
//              in-memory B+ tree. Default node sizes.
//   AlexIndex  ALEX (Ding et al., SIGMOD 2020), the best-known learned index
//              for data that changes. Default settings.
//
// Neither is given a special final search step here: their node layouts are
// part of their design, so each searches the way it was built to.
//
// Not included: the PGM-index's dynamic version. Its iterator only moves
// forward, so "largest key below q" has no efficient form.
#pragma once
#include "bits.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#ifndef NDEBUG
#define FT_RESTORE_ASSERT_DYN
#define NDEBUG
#endif
#include <cassert>
#include <tlx/container/btree_set.hpp>
#include "alex.h"
#ifdef FT_RESTORE_ASSERT_DYN
#undef NDEBUG
#undef FT_RESTORE_ASSERT_DYN
#include <cassert>
#endif

namespace ft {

class TlxSet {
public:
    void bulk_load(const std::vector<u64>& a) {
        s_.clear();
        s_.bulk_load(a.begin(), a.end());
    }
    void build(const std::vector<u64>& a) { bulk_load(a); }
    void insert(u64 k) { s_.insert(k); }
    bool predecessor(u64 q, u64& out) const {
        auto it = s_.lower_bound(q);
        if (it == s_.begin()) return false;
        --it;
        out = *it;
        return true;
    }
    std::size_t size() const { return s_.size(); }
    // Node counts are exact; per-node bytes follow TLX's layout (keys, child
    // pointers, a level and a fill count per node, sibling links per leaf).
    std::size_t bytes() const {
        const auto& st = s_.get_stats();
        std::size_t leaf = 16 + 4 + 2 * sizeof(void*) + st.leaf_slots * sizeof(u64);
        std::size_t inner = 16 + st.inner_slots * sizeof(u64) + (st.inner_slots + 1) * sizeof(void*);
        return st.leaves * leaf + st.inner_nodes * inner;
    }
    int levels(u64) const { return 1; }

private:
    tlx::btree_set<u64> s_;
};

class AlexIndex {
    using Alex = alex::Alex<u64, std::uint8_t>;
public:
    AlexIndex() : a_(std::make_unique<Alex>()) {}
    void bulk_load(const std::vector<u64>& a) {
        a_ = std::make_unique<Alex>();          // ALEX bulk-loads only into an empty index
        has_ = !a.empty();
        if (has_) { lo_ = a.front(); hi_ = a.back(); }
        // ALEX's bulk load fits a model to (max - min), which is 0 for one
        // key; below two keys, insert them one at a time instead.
        if (a.size() < 2) { for (u64 k : a) a_->insert(k, 0); return; }
        std::vector<std::pair<u64, std::uint8_t>> v(a.size());
        for (std::size_t i = 0; i < a.size(); ++i) v[i] = {a[i], 0};
        a_->bulk_load(v.data(), (int)v.size());
    }
    void build(const std::vector<u64>& a) { bulk_load(a); }
    void insert(u64 k) {
        a_->insert(k, 0);
        if (!has_) { lo_ = hi_ = k; has_ = true; }
        else { lo_ = k < lo_ ? k : lo_; hi_ = k > hi_ ? k : hi_; }
    }
    bool predecessor(u64 q, u64& out) const {
        // ALEX returns wrong answers for queries far outside the stored keys
        // (found by tests/test_dynamic.cpp), so those two cases are answered
        // here from the tracked smallest and largest key. Queries inside the
        // key range, which is almost all of them, go to ALEX.
        if (!has_ || q <= lo_) return false;
        if (q > hi_) { out = hi_; return true; }
        // ALEX finds the largest key <= x; the largest key < q is that for q - 1.
        auto it = a_->find_last_no_greater_than(q - 1);
        u64 k = it.key();
        if (k > q - 1) return false;
        out = k;
        return true;
    }
    std::size_t size() const { return a_->size(); }
    std::size_t bytes() const { return (std::size_t)(a_->model_size() + a_->data_size()); }
    int levels(u64) const { return 1; }

private:
    std::unique_ptr<Alex> a_;
    u64 lo_ = 0, hi_ = 0;
    bool has_ = false;
};

} // namespace ft
