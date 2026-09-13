// test_dynamic.cpp — differential test of every structure that accepts inserts.
//
// Oracle: std::set. Each trial bulk-loads half the keys (or none), then
// inserts the rest in random order (with some repeats, which must be ignored),
// and after every insert checks one query against the oracle.
//
// Counted per structure:
//   wrong    a wrong answer
//   crashed  an exception that ended the trial
// The test FAILS if ClusterJumpD or the TLX B+ tree is ever wrong or crashes.
// ALEX's problems are printed in full but do not fail the test: ALEX is
// third-party code, and its failures here are limits of ALEX itself, found by
// this test (see third_party/PATCHES.md):
//   * keys above 2^53 spaced closer than double precision, inserted into an
//     index that started empty, give wrong answers;
//   * a few tight clumps far apart, inserted from empty, can crash it.
//
// Build: g++ -O3 -std=c++17 -march=native -mbmi2 -Iinclude -Ithird_party/tlx
//            -Ithird_party/ALEX/src/core -static tests/test_dynamic.cpp
#include "cluster_jump_dynamic.hpp"
#include "dynamic_rivals.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace ft;

struct Count { long long checks = 0, wrong = 0, crashed = 0; };
static std::map<std::string, Count> g_count;

template <typename S>
static void trial(const char* name, int tr, const std::vector<u64>& initial, const std::vector<u64>& inserts,
                  const std::vector<u64>& queries) {
    Count& c = g_count[name];
    try {
        S s;
        s.bulk_load(initial);
        std::set<u64> oracle(initial.begin(), initial.end());
        for (std::size_t i = 0; i < inserts.size(); ++i) {
            s.insert(inserts[i]);
            oracle.insert(inserts[i]);
            u64 q = queries[i], got = 0, want = 0;
            auto it = oracle.lower_bound(q);
            bool hw = it != oracle.begin();
            if (hw) want = *std::prev(it);
            bool h = s.predecessor(q, got);
            ++c.checks;
            if (h != hw || (hw && got != want)) {
                if (++c.wrong <= 5)
                    std::printf("%s WRONG ANSWER: trial %d, initial=%zu, after %zu inserts, q=%llu got=%llu want=%llu\n",
                                name, tr, initial.size(), i + 1, (unsigned long long)q,
                                (unsigned long long)got, (unsigned long long)want);
            }
        }
    } catch (const std::exception& e) {
        ++c.crashed;
        std::printf("%s CRASHED: trial %d, initial=%zu, inserts=%zu: %s\n", name, tr, initial.size(),
                    inserts.size(), e.what());
    }
}

int main() {
    std::mt19937_64 rng(20260912);
    for (int tr = 0; tr < 240; ++tr) {
        std::size_t n = 1 + rng() % 6000;
        int mode = tr % 4;
        std::set<u64> s;
        while (s.size() < n) {
            u64 x = rng();
            if (mode == 1) x = (x % 100000) * 7;                            // dense
            if (mode == 2) x = 0xDEADBEEF00000000ull | (x & 0xFFFFFull);    // shared prefix
            if (mode == 3) x = ((x & 3) << 60) | ((x >> 8) & 0xFFF);        // tight clumps far apart
            s.insert(x);
        }
        std::vector<u64> all(s.begin(), s.end());
        std::shuffle(all.begin(), all.end(), rng);
        std::size_t half = (tr % 5 == 0) ? 0 : all.size() / 2;    // some trials start empty
        std::vector<u64> initial(all.begin(), all.begin() + half);
        std::sort(initial.begin(), initial.end());
        std::vector<u64> inserts(all.begin() + half, all.end());
        for (std::size_t i = 0; i < inserts.size() / 10; ++i) inserts.push_back(all[rng() % all.size()]);   // repeats
        std::shuffle(inserts.begin(), inserts.end(), rng);
        std::vector<u64> queries(inserts.size());
        for (auto& q : queries) {
            u64 k = all[rng() % all.size()];
            int pick = rng() % 4;
            q = pick == 0 ? k : pick == 1 ? k + 1 : pick == 2 ? k - 1 : rng();
        }
        trial<ClusterJumpD>("clusterjump_d", tr, initial, inserts, queries);
        trial<TlxSet>("tlx_btree", tr, initial, inserts, queries);
        trial<AlexIndex>("alex", tr, initial, inserts, queries);
    }
    bool ok = true;
    for (const auto& [name, c] : g_count) {
        std::printf("%-14s checks=%lld wrong=%lld crashed=%lld\n", name.c_str(), c.checks, c.wrong, c.crashed);
        if (name != "alex" && (c.wrong || c.crashed)) ok = false;
    }
    std::printf(ok ? "PASS (ClusterJumpD and TLX)\n" : "FAIL\n");
    return ok ? 0 : 1;
}
