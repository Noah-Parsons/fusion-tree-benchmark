// node_lat.cpp — Experiment 3, part 1: the cost of ONE node search, in cycles.
//
// Hardware counters are not available on this machine (WSL2 exposes no PMU;
// see the work log), so the dependency-chain argument of manual Part II.2 is
// tested by timing instead. Everything here fits in the L1 cache — 64 nodes
// of each type — so memory plays no part. What is left is the arithmetic and
// its dependency chain.
//
//   tput  independent searches: how many node searches per cycle the core
//         can overlap.
//   lat   each search's query depends on the previous search's answer: the
//         length of one search's dependency chain, which is what II.2
//         estimates at about 11 cycles for the fusion node.
//
// Cycles are derived from a calibration loop: a chain of dependent 1-cycle
// ADD instructions, timed before and after the measurements, gives the
// nanoseconds per cycle at whatever clock the core is actually running.
//
// Output: CSV on stdout
//   node,keys,mode,ns,cycles
//
// Run:   ./build/node_lat [reps=15] [cpu=-1]
#include "fusion_node.hpp"
#include "fusion_node_compact.hpp"
#include "fusion_node_wide.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

using namespace ft;

static volatile u64 g_zero = 0;
static volatile u64 g_sink = 0;

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

using clk = std::chrono::steady_clock;

// Nanoseconds per core cycle: 4 dependent ADDs per iteration, 1 cycle each.
static double ns_per_cycle() {
    const long N = 1L << 26;
    double best = 1e9;
    for (int t = 0; t < 5; ++t) {
        u64 x = g_zero + 1;
        auto t0 = clk::now();
        for (long i = 0; i < N; ++i)
            asm volatile("add %0, %0\n\tadd %0, %0\n\tadd %0, %0\n\tadd %0, %0" : "+r"(x));
        auto t1 = clk::now();
        g_sink = x;
        best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / (4.0 * N));
    }
    return best;
}

// B-tree node scans, the same code as BTree::node_rank in structures.hpp
// (explicit AVX2 for the branch-free scan), without the child indices: a
// single node search never reads them.
template <int B, bool Branchless>
struct BTreeNodeScan {
    u64 key[B];
    int n;
    void build(const u64* k, int cnt) { n = cnt; for (int i = 0; i < B; ++i) key[i] = i < cnt ? k[i] : 0; }
    int rank(u64 q) const {
        int r = 0;
        if constexpr (Branchless) {
#if defined(__AVX2__)
            if constexpr (B % 4 == 0) {
                const __m256i flip = _mm256_set1_epi64x((long long)0x8000000000000000ull);
                const __m256i qv = _mm256_xor_si256(_mm256_set1_epi64x((long long)q), flip);
                unsigned m = 0;
                for (int i = 0; i < B; i += 4) {
                    __m256i k = _mm256_xor_si256(
                        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&key[i])), flip);
                    m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))) << i;
                }
                return popcount(m & ((1u << n) - 1));
            }
#endif
            for (int i = 0; i < B; ++i) r += (i < n) & (key[i] < q);
        } else {
            while (r < n && key[r] < q) ++r;
        }
        return r;
    }
};

constexpr int NODES = 64;
constexpr int QUERIES = 1 << 14;

template <typename Node, int K>
static void measure(const char* name, int reps, double npc, std::mt19937_64& rng) {
    std::vector<Node> nodes(NODES);
    for (auto& nd : nodes) {
        std::set<u64> s;
        while ((int)s.size() < K) s.insert(rng());
        std::vector<u64> k(s.begin(), s.end());
        nd.build(k.data(), K);
    }
    std::vector<u64> qs(QUERIES);
    std::vector<int> idx(QUERIES);
    for (int i = 0; i < QUERIES; ++i) { qs[i] = rng(); idx[i] = (int)(rng() % NODES); }

    for (int mode = 0; mode < 2; ++mode) {
        std::vector<double> t;
        for (int rep = 0; rep < reps + 1; ++rep) {      // rep 0 is warm-up
            u64 sum = 0;
            auto t0 = clk::now();
            if (mode == 0) {
                for (int i = 0; i < QUERIES; ++i) sum += (u64)nodes[idx[i]].rank(qs[i]);
            } else {
                const u64 z = g_zero;
                u64 r = 0;
                for (int i = 0; i < QUERIES; ++i) {
                    r = (u64)nodes[idx[i]].rank(qs[i] ^ (r & z));
                    sum += r;
                }
            }
            auto t1 = clk::now();
            g_sink = sum;
            if (rep > 0) t.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / QUERIES);
        }
        std::sort(t.begin(), t.end());
        double med = t[t.size() / 2];
        std::printf("%s,%d,%s,%.3f,%.1f\n", name, K, mode ? "lat" : "tput", med, med / npc);
    }
}

int main(int argc, char** argv) {
    int reps = (argc > 1) ? std::atoi(argv[1]) : 15;
    int cpu  = (argc > 2) ? std::atoi(argv[2]) : -1;
    bool pinned = cpu >= 0 && pin_to_cpu(cpu);

    double npc0 = ns_per_cycle();
    std::mt19937_64 rng(20260910);
    std::printf("node,keys,mode,ns,cycles\n");
    measure<BTreeNodeScan<8, false>, 8>("btree_scan_early_exit", reps, npc0, rng);
    measure<BTreeNodeScan<8, true>, 8>("btree_scan_branchfree", reps, npc0, rng);
    measure<BTreeNodeScan<16, true>, 16>("btree_scan_branchfree", reps, npc0, rng);
    measure<FusionNode<8>, 8>("fusion_original", reps, npc0, rng);
    measure<FusionNode<8, true>, 8>("fusion_branchfree", reps, npc0, rng);
    measure<FusionNodeCompact, 8>("fusion_compact", reps, npc0, rng);
    measure<FusionNodeWide, 16>("fusion_wide_avx2", reps, npc0, rng);
    double npc1 = ns_per_cycle();
    std::fprintf(stderr, "cpu=%d pinned=%d clock=%.2f GHz before, %.2f GHz after (cycles use 'before')\n",
                 cpu, pinned ? 1 : 0, 1.0 / npc0, 1.0 / npc1);
    return 0;
}
