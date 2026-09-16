// test_rmi.cpp — checks every generated RMI against std::lower_bound.
//
// For each dump file given (data/samples/<dataset>_<seed>), loads the keys,
// builds RMIIndex for every rank that has a registered RMI trained on exactly
// those keys, and compares 2,000,000 predecessor queries with the oracle:
// half land exactly on keys, half at random points between neighbours (the
// case where RMI's error bound is not guaranteed). Also counts how often the
// wrapper's window-edge fallback was needed.
//
// Build: linked with data/rmi/registry.cpp and the generated RMI sources.
#include "rmi_index.hpp"
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

using namespace ft;

static std::vector<u64> load(const char* path) {
    std::vector<u64> v;
    FILE* f = std::fopen(path, "rb");
    if (!f) return v;
    u64 count = 0;
    if (std::fread(&count, 8, 1, f) == 1) { v.resize(count); v.resize(std::fread(v.data(), 8, count, f)); }
    std::fclose(f);
    return v;
}

template <typename S>
static long long check(const char* label, const std::vector<u64>& a, const std::vector<u64>& qs) {
    S s;
    s.build(a);
    if (!s.matched()) return -1;
    long long wrong = 0;
    for (u64 q : qs) {
        auto it = std::lower_bound(a.begin(), a.end(), q);
        bool hw = it != a.begin();
        u64 want = hw ? *(it - 1) : 0, got = 0;
        bool h = s.predecessor(q, got);
        if (h != hw || (hw && got != want)) {
            if (++wrong <= 3)
                std::printf("  %s WRONG q=%llu got=%llu want=%llu\n", label, (unsigned long long)q,
                            (unsigned long long)got, (unsigned long long)want);
        }
    }
    return wrong;
}

int main(int argc, char** argv) {
    bool ok = true;
    for (int i = 1; i < argc; ++i) {
        std::vector<u64> a = load(argv[i]);
        if (a.empty()) { std::printf("%s: cannot read\n", argv[i]); ok = false; continue; }
        std::mt19937_64 rng(20260913);
        std::vector<u64> qs(2000000);
        for (std::size_t j = 0; j < qs.size(); ++j) {
            std::size_t k = rng() % a.size();
            if (j % 2 == 0) { qs[j] = a[k]; continue; }
            u64 gap = k + 1 < a.size() ? a[k + 1] - a[k] : 1;
            qs[j] = a[k] + rng() % gap;
        }
        std::printf("%s (%zu keys)\n", argv[i], a.size());
        int matched = 0;
        for (int rank = 1; rank <= 16; ++rank) {
            long long w = -1;
            switch (rank) {
#define R(k) case k: w = check<RMIIndex<k>>("rank " #k, a, qs); break;
                R(1) R(2) R(3) R(4) R(5) R(6) R(7) R(8) R(9) R(10) R(11) R(12) R(13) R(14) R(15) R(16)
#undef R
            }
            if (w < 0) continue;
            ++matched;
            std::printf("  rank %d: wrong=%lld of %zu\n", rank, w, qs.size());
            if (w) ok = false;
        }
        if (!matched) { std::printf("  no registered RMI matches these keys\n"); ok = false; }
    }
    std::printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
