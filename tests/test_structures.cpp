// test_structures.cpp — differential test of every full structure.
//
// Oracle: std::lower_bound over the same sorted vector. Every structure must
// agree with it on every query, or the run fails.
//
// Build: make test
#include "structures.hpp"
#include "splus_tree.hpp"
#include "radix_jump.hpp"
#include "spline_index.hpp"
#include "cluster_jump.hpp"
#include "learned_indexes.hpp"
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

struct Tally { long long checks = 0, failures = 0; };

template <typename S>
static void check(const char* name, const S& st, const std::vector<u64>& a,
                  const std::vector<u64>& qs, Tally& t) {
    for (u64 q : qs) {
        u64 want = 0, got = 0;
        bool hw = oracle_pred(a, q, want);
        bool h = st.predecessor(q, got);
        ++t.checks;
        if (h != hw || (hw && got != want)) {
            ++t.failures;
            if (t.failures < 8)
                std::printf("%s mismatch n=%zu q=%llu got=%llu want=%llu\n", name, a.size(),
                            (unsigned long long)q, (unsigned long long)got, (unsigned long long)want);
        }
    }
}

int main() {
    std::mt19937_64 rng(20260909);
    Tally t;

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

        std::vector<u64> qs(4000);
        for (int i = 0; i < 4000; ++i) {
            int pick = i % 4;
            if (pick == 0) qs[i] = a[rng() % a.size()];
            else if (pick == 1) qs[i] = a[rng() % a.size()] + 1;
            else if (pick == 2) { qs[i] = a[rng() % a.size()]; if (qs[i]) --qs[i]; }
            else qs[i] = rng();
        }

        SortedArray sa;   sa.build(a);   check("SortedArray", sa, a, qs, t);
        BTree8 b1;        b1.build(a);   check("btree8", b1, a, qs, t);
        BTree8BL b2;      b2.build(a);   check("btree8_bl", b2, a, qs, t);
        BTree8A64 b3;     b3.build(a);   check("btree8_a64", b3, a, qs, t);
        BTree8BLA64 b4;   b4.build(a);   check("btree8_bl_a64", b4, a, qs, t);
        BTree16BLA64 b5;  b5.build(a);   check("btree16_bl_a64", b5, a, qs, t);
        Fusion8 f1;       f1.build(a);   check("fusion8", f1, a, qs, t);
        Fusion8BF f2;     f2.build(a);   check("fusion8_bf", f2, a, qs, t);
        Fusion8C f3;      f3.build(a);   check("fusion8_c", f3, a, qs, t);
        Fusion16W f4;     f4.build(a);   check("fusion16_w", f4, a, qs, t);
        SPlus8 s1;        s1.build(a);   check("splus8", s1, a, qs, t);
        SPlus16 s2;       s2.build(a);   check("splus16", s2, a, qs, t);
        RadixJump rj;     rj.build(a);   check("radixjump", rj, a, qs, t);
        Spline8 p1;       p1.build(a);   check("spline8", p1, a, qs, t);
        Spline16 p2;      p2.build(a);   check("spline16", p2, a, qs, t);
        Spline32 p3;      p3.build(a);   check("spline32", p3, a, qs, t);
        ClusterJump cj;   cj.build(a);   check("clusterjump", cj, a, qs, t);
        RS18E32 l1;       l1.build(a);   check("rs18_e32", l1, a, qs, t);
        RS22E8 l2;        l2.build(a);   check("rs22_e8", l2, a, qs, t);
        PGM16 l3;         l3.build(a);   check("pgm16", l3, a, qs, t);
        PGM128 l4;        l4.build(a);   check("pgm128", l4, a, qs, t);
    }
    std::printf("checks=%lld failures=%lld\n", t.checks, t.failures);
    return t.failures == 0 ? 0 : 1;
}
