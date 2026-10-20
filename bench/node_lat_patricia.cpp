// node_lat_patricia.cpp — the kill gate of results/breakthrough/PLAN.md.
//
// Same harness as node_lat.cpp (64 nodes in L1, 2^14 queries, median of
// reps, cycles from a chain of dependent ADDs), for PatriciaNode's step and
// rank, with the 16-key branch-free B-tree node re-measured alongside.
//
// Before any timing, every node type is checked on every query: step against
// an explicit Patricia descent, rank against std::lower_bound, on uniform,
// comb-shaped, consecutive and clustered key sets. Any mismatch aborts.
//
// Output: CSV on stdout
//   node,keys,mode,ns,cycles
//
// Run:   ./build/node_lat_patricia [reps=15] [cpu=-1]
#include "patricia_node.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#endif

using namespace ft;

static volatile u64 g_zero = 0;
static volatile u64 g_sink = 0;

static bool pin_to_cpu(int cpu) {
#if defined(_WIN32)
    return SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << cpu) != 0;
#else
    (void)cpu; return false;
#endif
}

using clk = std::chrono::steady_clock;

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

// The 16-key branch-free B-tree node scan, as in node_lat.cpp.
struct BTree16 {
    u64 key[16];
    int n;
    void build(const u64* k, int cnt) { n = cnt; for (int i = 0; i < 16; ++i) key[i] = i < cnt ? k[i] : 0; }
    int rank(u64 q) const {
        const __m256i flip = _mm256_set1_epi64x((long long)0x8000000000000000ull);
        const __m256i qv = _mm256_xor_si256(_mm256_set1_epi64x((long long)q), flip);
        unsigned m = 0;
        for (int i = 0; i < 16; i += 4) {
            __m256i k = _mm256_xor_si256(
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&key[i])), flip);
            m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))) << i;
        }
        return popcount(m & ((1u << n) - 1));
    }
};

// ---- correctness -----------------------------------------------------------

static int patricia_descent(const u64* k, int cnt, u64 q) {
    int lo = 0, hi = cnt;
    while (hi - lo > 1) {
        int d = msb(k[lo] ^ k[hi - 1]);
        int m = lo;
        while (!((k[m] >> d) & 1)) ++m;
        if ((q >> d) & 1) lo = m; else hi = m;
    }
    return lo;
}

static std::vector<u64> make_keys(int shape, int K, std::mt19937_64& rng) {
    std::set<u64> s;
    while ((int)s.size() < K) {
        u64 v = rng();
        switch (shape) {
        case 0: break;                                              // uniform
        case 1: v = (1ull << (rng() % 64)) | (rng() & 3); break;    // comb
        case 2: v = 0x123456789ull + s.size(); break;               // consecutive
        case 3: v = ((rng() % 3) << 60) | ((rng() % 4) << 30) | (rng() & 0xFF); break; // nested clusters
        }
        s.insert(v);
    }
    return {s.begin(), s.end()};
}

template <int K>
static bool check(std::mt19937_64& rng) {
    long bad = 0, checks = 0;
    for (int shape = 0; shape < 4; ++shape)
        for (int trial = 0; trial < 2000; ++trial) {
            int cnt = 1 + (int)(rng() % K);
            std::vector<u64> k = make_keys(shape, cnt, rng);
            PatriciaNode<K> nd;
            nd.build(k.data(), cnt);
            for (int t = 0; t < 64; ++t) {
                u64 q;
                switch (t % 4) {
                case 0: q = rng(); break;
                case 1: q = k[rng() % cnt]; break;
                case 2: q = k[rng() % cnt] + 1; break;
                default: q = k[rng() % cnt] - 1; break;
                }
                if (t == 60) q = 0;
                if (t == 61) q = ~0ull;
                int want_step = patricia_descent(k.data(), cnt, q);
                int want_rank = (int)(std::lower_bound(k.begin(), k.end(), q) - k.begin());
                bad += nd.step(q) != want_step;
                bad += nd.rank(q) != want_rank;
                checks += 2;
            }
        }
    std::fprintf(stderr, "PatriciaNode<%d>: %ld checks, %ld wrong\n", K, checks, bad);
    return bad == 0;
}

// ---- timing ----------------------------------------------------------------

constexpr int NODES = 64;
constexpr int QUERIES = 1 << 14;

template <typename Node, int K, typename F>
static void measure(const char* name, int reps, double npc, std::mt19937_64& rng, F op) {
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
        for (int rep = 0; rep < reps + 1; ++rep) {
            u64 sum = 0;
            auto t0 = clk::now();
            if (mode == 0) {
                for (int i = 0; i < QUERIES; ++i) sum += (u64)op(nodes[idx[i]], qs[i]);
            } else {
                const u64 z = g_zero;
                u64 r = 0;
                for (int i = 0; i < QUERIES; ++i) {
                    r = (u64)op(nodes[idx[i]], qs[i] ^ (r & z));
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

    std::mt19937_64 crng(20261015);
    if (!check<16>(crng) || !check<32>(crng)) { std::fprintf(stderr, "ABORT: wrong answers\n"); return 1; }

    double npc0 = ns_per_cycle();
    std::mt19937_64 rng(20260910);
    std::printf("node,keys,mode,ns,cycles\n");
    auto rk = [](const auto& nd, u64 q) { return nd.rank(q); };
    auto st = [](const auto& nd, u64 q) { return nd.step(q); };
    measure<BTree16, 16>("btree_scan_branchfree", reps, npc0, rng, rk);
    measure<PatriciaNode<16>, 16>("patricia_step", reps, npc0, rng, st);
    measure<PatriciaNode<16>, 16>("patricia_rank", reps, npc0, rng, rk);
    measure<PatriciaNode<32>, 32>("patricia_step", reps, npc0, rng, st);
    measure<PatriciaNode<32>, 32>("patricia_rank", reps, npc0, rng, rk);
    double npc1 = ns_per_cycle();
    std::fprintf(stderr, "cpu=%d pinned=%d clock=%.2f GHz before, %.2f GHz after (cycles use 'before')\n",
                 cpu, pinned ? 1 : 0, 1.0 / npc0, 1.0 / npc1);
    return 0;
}
