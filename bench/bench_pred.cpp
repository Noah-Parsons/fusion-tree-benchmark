// bench_pred.cpp — Experiment 2: the timing campaign.
//
// Measures nanoseconds per predecessor query for each structure across a
// range of input sizes.
//
// Five rules are being obeyed here, and each of them exists because ignoring
// it produces a wrong number:
//
//   1. NOTHING IS OPTIMISED AWAY. Every query result is folded into a
//      checksum that is printed at the end. The warm-up result is written to
//      a volatile, so the warm-up pass cannot be deleted either.
//   2. QUERIES ARE PRE-GENERATED. Random number generation inside the timed
//      region measures the random number generator.
//   3. EACH SIZE IS MEASURED MANY TIMES AND THE MEDIAN REPORTED. A mean is
//      dragged around by any single interrupted run; a median is not.
//   4. A WARM-UP PASS RUNS FIRST AND IS DISCARDED, so the first-touch page
//      faults and the cold instruction cache are not charged to the result.
//   5. THE ORDER OF STRUCTURES IS SHUFFLED ON EVERY REPETITION, so no
//      structure always runs straight after the same neighbour's cache state.
//
// Two modes:
//
//   tput  queries are independent, so the CPU overlaps several at once.
//         This measures THROUGHPUT: how many queries per second.
//   lat   each query is made to depend on the previous answer (XOR with a
//         zero the compiler cannot prove is zero). The CPU must finish one
//         query before starting the next. This measures LATENCY, which is
//         what the dependency-chain argument in the manual (Part II.2) is
//         actually about. Query values and checksums are identical to tput.
//
// On a hybrid CPU (performance + efficiency cores) always pin to a known
// performance core; an unpinned run can migrate between core types mid-run.
//
// Output: CSV on stdout, one row per (structure, n, repetition):
//   structure,n,rep,ns_per_query,checksum
// Configuration is printed to stderr.
//
// Run:   ./build/bench_pred [reps=15] [max_log=22] [cpu=-1] [tput|lat]
#include "structures.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

using namespace ft;

static volatile u64 g_sink;       // warm-up results land here
static volatile u64 g_zero = 0;   // a zero the optimiser cannot see through

static bool pin_to_cpu(int cpu) {
#if defined(_WIN32)
    return SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << cpu) != 0;
#elif defined(__linux__)
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
    return sched_setaffinity(0, sizeof(set), &set) == 0;
#else
    (void)cpu; return false;
#endif
}

// Draw, sort, drop duplicates, top up. With no duplicate draws (probability
// about n^2 / 2^65) this consumes exactly the same random numbers as the
// earlier std::set version, so the key sets are identical, but it is far
// faster at n = 2^25.
static std::vector<u64> make_sorted_keys(std::size_t n, std::mt19937_64& rng) {
    std::vector<u64> v;
    v.reserve(n);
    while (v.size() < n) {
        while (v.size() < n) v.push_back(rng());
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }
    return v;
}

template <typename S>
static void warm(const S& st, const std::vector<u64>& queries) {
    u64 sink = 0;
    for (u64 q : queries) { u64 o = 0; if (st.predecessor(q, o)) sink ^= o; }
    g_sink = sink;
}

// One timed pass. Returns ns per query; the checksum goes out via `checksum`.
template <typename S>
static double time_once(const S& st, const std::vector<u64>& queries,
                        bool latency, u64& checksum) {
    checksum = 0;
    auto t0 = std::chrono::steady_clock::now();
    if (!latency) {
        for (u64 q : queries) {
            u64 o = 0;
            if (st.predecessor(q, o)) checksum ^= o;
        }
    } else {
        const u64 z = g_zero;
        u64 dep = 0;
        for (u64 q0 : queries) {
            u64 o = 0;
            bool hit = st.predecessor(q0 ^ dep, o);
            if (hit) checksum ^= o;
            dep = (o + hit) & z;          // always 0, but only at run time
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count()
           / (double)queries.size();
}

int main(int argc, char** argv) {
    int reps    = (argc > 1) ? std::atoi(argv[1]) : 15;
    int max_log = (argc > 2) ? std::atoi(argv[2]) : 22;
    int cpu     = (argc > 3) ? std::atoi(argv[3]) : -1;
    bool latency = (argc > 4) && std::strcmp(argv[4], "lat") == 0;
    std::size_t nq = 1u << 16;

    bool pinned = cpu >= 0 && pin_to_cpu(cpu);
    if (cpu >= 0 && !pinned) std::fprintf(stderr, "warning: could not pin to cpu %d\n", cpu);
#ifdef FT_USE_PEXT
    const char* sketch = "pext";
#else
    const char* sketch = "loop";
#endif
    std::fprintf(stderr, "mode=%s cpu=%d pinned=%d sketch=%s reps=%d max_log=%d\n",
                 latency ? "lat" : "tput", cpu, pinned ? 1 : 0, sketch, reps, max_log);

    std::mt19937_64 rng(20260909);
    std::mt19937 order_rng(12345);
    std::printf("structure,n,rep,ns_per_query,checksum\n");

    const char* names[4] = {"sorted_array", "btree8", "btree8_bl", "fusion8"};

    for (int lg = 8; lg <= max_log; ++lg) {
        std::size_t n = (std::size_t)1 << lg;
        std::vector<u64> keys = make_sorted_keys(n, rng);

        std::vector<u64> queries(nq);
        for (auto& q : queries) q = rng();

        SortedArray sa;        sa.build(keys);
        BTree<8> bt;           bt.build(keys);
        BTree<8, true> bb;     bb.build(keys);
        FusionTree<8> ftree;   ftree.build(keys);

        warm(sa, queries); warm(bt, queries); warm(bb, queries); warm(ftree, queries);

        std::array<int, 4> order = {0, 1, 2, 3};
        for (int rep = 0; rep < reps; ++rep) {
            std::shuffle(order.begin(), order.end(), order_rng);
            for (int which : order) {
                u64 cs = 0;
                double ns = 0;
                switch (which) {
                    case 0: ns = time_once(sa,    queries, latency, cs); break;
                    case 1: ns = time_once(bt,    queries, latency, cs); break;
                    case 2: ns = time_once(bb,    queries, latency, cs); break;
                    case 3: ns = time_once(ftree, queries, latency, cs); break;
                }
                std::printf("%s,%zu,%d,%.4f,%llu\n", names[which], n, rep, ns,
                            (unsigned long long)cs);
            }
        }
    }
    return 0;
}
