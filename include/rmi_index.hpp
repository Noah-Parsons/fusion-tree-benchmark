// rmi_index.hpp — RMI (Kraska et al., SIGMOD 2018) wrapped for the race.
//
// An RMI is not a library: the reference tool (github.com/learnedsystems/RMI)
// trains models on one specific key file and generates C++ for them. Every
// generated RMI is a namespace with load(), cleanup() and
//     uint64_t lookup(uint64_t key, size_t* err)
// which returns a predicted position and the largest error seen on the
// training keys.
//
// The generated code lives in data/rmi/ (not in git). A generated registry
// (data/rmi/registry.cpp) lists every RMI with a fingerprint of the keys it
// was trained on; without it, bench/rmi_none.cpp provides an empty registry.
// RMIIndex<Rank>::build() picks the registered RMI of that rank whose
// fingerprint (key count, first key, last key) matches the keys it is given.
// If none matches, as at every size but the one trained for, it falls back to
// a plain search, and those timings mean nothing.
//
// ROBUSTNESS. RMI's error bound is measured on the stored keys only. A query
// between keys can, near a boundary between last-level models, be predicted
// outside [guess - err, guess + err]. So after the window search RMIIndex
// checks the two window edges and, if the answer lies beyond one, searches
// the rest of the array. SOSD's "robust" RMI wrapper exists for the same
// reason. The check is two comparisons and a branch that is almost never
// taken. The other structures do not need it: their bounds hold for any query.
//
// Fairness: the same final search step as everything else (detail::last_mile).
#pragma once
#include "learned_indexes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ft {

struct RMIEntry {
    const char* dataset;           // for reports only
    unsigned long long seed;       // for reports only
    int rank;                      // position on the dataset's Pareto list, by size (1 = smallest)
    const char* architecture;      // e.g. "linear_spline,linear 2^20"
    std::size_t n;                 // fingerprint of the training keys
    std::uint64_t first, last;
    bool (*load)(char const* data_path);
    void (*cleanup)();
    std::uint64_t (*lookup)(std::uint64_t key, std::size_t* err);
    std::size_t size_bytes;
};

// Defined in data/rmi/registry.cpp or bench/rmi_none.cpp.
const RMIEntry* rmi_registry(std::size_t& count);
const char* rmi_data_path();

template <int Rank>
class RMIIndex {
public:
    RMIIndex() = default;
    RMIIndex(const RMIIndex&) = delete;
    RMIIndex& operator=(const RMIIndex&) = delete;
    ~RMIIndex() { release(); }

    void build(const std::vector<u64>& a) {
        release();
        keys_ = detail::flipped_copy(a);
        n_ = a.size();
        std::size_t count = 0;
        const RMIEntry* reg = rmi_registry(count);
        for (std::size_t i = 0; i < count && n_; ++i) {
            const RMIEntry& e = reg[i];
            if (e.rank == Rank && e.n == n_ && e.first == a.front() && e.last == a.back()) {
                if (!e.load(rmi_data_path())) break;
                entry_ = &e;
                break;
            }
        }
    }

    bool predecessor(u64 q, u64& out) const {
        std::size_t lo = 0, len = n_;
        if (entry_) {
            std::size_t err = 0;
            std::size_t g = (std::size_t)entry_->lookup(q, &err);
            lo = g > err ? g - err : 0;
            lo = lo < n_ ? lo : n_;
            std::size_t hi = g + err + 2 < n_ ? g + err + 2 : n_;
            len = hi > lo ? hi - lo : 0;
        }
        std::size_t lb = detail::last_mile(keys_, lo, len, q);
        if (entry_) {
            const long long qs = (long long)(q ^ detail::LI_FLIP);
            if (lb == lo && lo > 0 && (long long)keys_[lo - 1] >= qs) {
                lb = detail::last_mile(keys_, 0, lo, q);                      // answer is left of the window
            } else if (lb == lo + len && lb < n_ && (long long)keys_[lb] < qs) {
                lb = detail::last_mile(keys_, lb, n_ - lb, q);                // answer is right of the window
            }
        }
        if (lb == 0) return false;
        out = keys_[lb - 1] ^ detail::LI_FLIP;
        return true;
    }

    bool matched() const { return entry_ != nullptr; }
    std::size_t size() const { return n_; }
    std::size_t bytes() const { return (n_ + 8) * sizeof(u64) + (entry_ ? entry_->size_bytes : 0); }
    int levels(u64) const { return 1; }

private:
    void release() {
        if (entry_) { entry_->cleanup(); entry_ = nullptr; }
        if (keys_) { _mm_free(keys_); keys_ = nullptr; }
    }
    u64* keys_ = nullptr;
    std::size_t n_ = 0;
    const RMIEntry* entry_ = nullptr;
};

} // namespace ft
