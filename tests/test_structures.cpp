// test_structures.cpp — differential test of the three full structures.
//
// Oracle: std::lower_bound over the same sorted vector. Every structure must
// agree with it on every query, or the run fails.
//
// Build: g++ -O2 -std=c++20 -Iinclude tests/test_structures.cpp -o build/test_structures
#include "structures.hpp"
#include <algorithm>
#include <cstdio>
#include <random>
#include <set>

using namespace ft;

static bool oracle_pred(const std::vector<u64>& a, u64 q, u64& out) {
    auto it = std::lower_bound(a.begin(), a.end(), q);
    if (it == a.begin()) return false;
    out = *(it - 1);
    return true;
}

int main() {
    std::mt19937_64 rng(20260909);
    long long checks = 0, failures = 0;

    for (int trial = 0; trial < 300; ++trial) {
        std::size_t n = 1 + rng() % 5000;
        std::set<u64> s;
        int mode = trial % 3;
        while (s.size() < n) {
            u64 x = rng();
            if (mode == 1) x = (x % 100000) * 7;          // dense, clustered
            if (mode == 2) x = 0xDEADBEEF00000000ull | (x & 0xFFFFFull); // shared prefix
            s.insert(x);
        }
        std::vector<u64> a(s.begin(), s.end());

        SortedArray sa;   sa.build(a);
        BTree<8> bt;      bt.build(a);
        BTree<8, true> bb; bb.build(a);
        FusionTree<8> ftree; ftree.build(a);

        for (int i = 0; i < 4000; ++i) {
            u64 q;
            int pick = i % 4;
            if (pick == 0) q = a[rng() % a.size()];
            else if (pick == 1) q = a[rng() % a.size()] + 1;
            else if (pick == 2) { q = a[rng() % a.size()]; if (q) --q; }
            else q = rng();

            u64 want = 0, got = 0;
            bool hw = oracle_pred(a, q, want);

            bool h1 = sa.predecessor(q, got);
            ++checks;
            if (h1 != hw || (hw && got != want)) {
                ++failures;
                if (failures < 5) std::printf("SortedArray mismatch q=%llu\n", (unsigned long long)q);
            }

            got = 0;
            bool h2 = bt.predecessor(q, got);
            ++checks;
            if (h2 != hw || (hw && got != want)) {
                ++failures;
                if (failures < 5) std::printf("BTree mismatch q=%llu got=%llu want=%llu\n",
                                              (unsigned long long)q, (unsigned long long)got,
                                              (unsigned long long)want);
            }

            got = 0;
            bool h4 = bb.predecessor(q, got);
            ++checks;
            if (h4 != hw || (hw && got != want)) {
                ++failures;
                if (failures < 5) std::printf("BTree(branchless) mismatch q=%llu got=%llu want=%llu\n",
                                              (unsigned long long)q, (unsigned long long)got,
                                              (unsigned long long)want);
            }

            got = 0;
            bool h3 = ftree.predecessor(q, got);
            ++checks;
            if (h3 != hw || (hw && got != want)) {
                ++failures;
                if (failures < 5) std::printf("FusionTree mismatch n=%zu q=%llu got=%llu want=%llu\n",
                                              n, (unsigned long long)q, (unsigned long long)got,
                                              (unsigned long long)want);
            }
        }
    }
    std::printf("checks=%lld failures=%lld\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
