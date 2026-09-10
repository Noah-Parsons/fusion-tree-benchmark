// bench_pred.cpp — Experiment 2: the timing campaign.
//
// Measures nanoseconds per predecessor query for each structure across a
// range of input sizes.
//
// Four rules are being obeyed here, and each of them exists because ignoring
// it produces a wrong number:
//
//   1. NOTHING IS OPTIMISED AWAY. Every query result is folded into a
//      checksum that is printed at the end. Without this the compiler is
//      entitled to delete the entire loop, and it will.
//   2. QUERIES ARE PRE-GENERATED. Random number generation inside the timed
//      region measures the random number generator.
//   3. EACH SIZE IS MEASURED MANY TIMES AND THE MEDIAN REPORTED. A mean is
//      dragged around by any single interrupted run; a median is not.
//   4. A WARM-UP PASS RUNS FIRST AND IS DISCARDED, so the first-touch page
//      faults and the cold instruction cache are not charged to the result.
//
// Output: CSV on stdout, one row per (structure, n, repetition):
//   structure,n,rep,ns_per_query,checksum
//
// Build: g++ -O3 -std=c++20 -mbmi2 -Iinclude bench/bench_pred.cpp -o build/bench_pred
// Run:   ./build/bench_pred > results/timing_raw.csv
#include "structures.hpp"
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include <vector>

using namespace ft;

static std::vector<u64> make_sorted_keys(std::size_t n, std::mt19937_64& rng) {
    std::set<u64> s;
    while (s.size() < n) s.insert(rng());
    return std::vector<u64>(s.begin(), s.end());
}

template <typename S>
static void run(const char* name, const S& st, const std::vector<u64>& queries,
                std::size_t n, int reps) {
    // Warm-up, discarded.
    u64 sink = 0;
    for (u64 q : queries) { u64 o = 0; if (st.predecessor(q, o)) sink ^= o; }

    for (int rep = 0; rep < reps; ++rep) {
        u64 checksum = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (u64 q : queries) {
            u64 o = 0;
            if (st.predecessor(q, o)) checksum ^= o;
        }
        auto t1 = std::chrono::steady_clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        std::printf("%s,%zu,%d,%.4f,%llu\n", name, n, rep,
                    ns / (double)queries.size(),
                    (unsigned long long)(checksum ^ sink * 0));
    }
}

int main(int argc, char** argv) {
    int reps    = (argc > 1) ? std::atoi(argv[1]) : 15;
    int max_log = (argc > 2) ? std::atoi(argv[2]) : 22;
    std::size_t nq = 1u << 16;

    std::mt19937_64 rng(20260909);
    std::printf("structure,n,rep,ns_per_query,checksum\n");

    for (int lg = 8; lg <= max_log; ++lg) {
        std::size_t n = (std::size_t)1 << lg;
        std::vector<u64> keys = make_sorted_keys(n, rng);

        std::vector<u64> queries(nq);
        for (auto& q : queries) q = rng();

        SortedArray sa;      sa.build(keys);
        BTree<8> bt;         bt.build(keys);
        FusionTree<8> ftree; ftree.build(keys);

        run("sorted_array", sa,    queries, n, reps);
        run("btree8",       bt,    queries, n, reps);
        run("fusion8",      ftree, queries, n, reps);
    }
    return 0;
}
