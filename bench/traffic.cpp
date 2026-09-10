// traffic.cpp — Experiment 3, part 2: how much memory does a query touch?
//
// Without hardware counters (see node_lat.cpp) the memory side of the
// mechanism is measured by instrumentation. Every structure reports the
// exact bytes each predecessor query reads (the touch() methods), and this
// program turns those into:
//
//   levels        nodes visited per query, and the share of queries that
//                 visit the most common number of nodes (a loop whose trip
//                 count varies from query to query is a branch the CPU
//                 cannot predict)
//   lines         distinct 64-byte cache lines touched per query — exact
//   bytes         bytes read per query — exact
//   *_mpq         MODELLED misses per query at each cache level and in the
//                 second-level TLB. The model replays the address stream
//                 through LRU set-associative caches sized like one
//                 performance core of the Core Ultra 7 255HX:
//                   L0 48 KB 12-way, L1 192 KB 8-way, L2 3 MB 12-way,
//                   L3 30 MB 12-way (all of it, as if the core were alone),
//                   STLB 2048 entries 16-way over 4 KB pages.
//                 The associativities are assumptions and there is no
//                 prefetcher, so treat these as a model in the manner of
//                 cachegrind, not as measurements.
//
// Keys and queries are drawn with the same seed and order as bench_pred, so
// the structures are the same ones that were timed. A warm-up pass runs
// before the counted pass, as in the benchmark.
//
// Output: CSV on stdout
//   structure,n,levels_mean,levels_modal_share,lines_per_query,bytes_per_query,
//   l0_mpq,l1_mpq,l2_mpq,l3_mpq,stlb_mpq
//
// Run:   ./build/traffic [max_log=22]
#include "structures.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <vector>

using namespace ft;

// One level of LRU set-associative cache over fixed-size blocks.
class Cache {
public:
    Cache(std::size_t bytes, int ways, int block_bits)
        : ways_(ways), block_bits_(block_bits),
          sets_(bytes >> block_bits) {
        sets_ /= (std::size_t)ways;
        tags_.assign(sets_ * ways_, ~0ull);
    }
    // Returns true on a hit. On a miss the block is filled.
    bool access(std::uintptr_t addr) {
        u64 block = (u64)addr >> block_bits_;
        u64* set = &tags_[(block % sets_) * ways_];
        for (int w = 0; w < ways_; ++w) {
            if (set[w] == block) {                      // hit: move to front
                for (int k = w; k > 0; --k) set[k] = set[k - 1];
                set[0] = block;
                return true;
            }
        }
        for (int k = ways_ - 1; k > 0; --k) set[k] = set[k - 1];
        set[0] = block;
        return false;
    }
private:
    int ways_, block_bits_;
    std::size_t sets_;
    std::vector<u64> tags_;
};

struct Hierarchy {
    Cache l0{48u << 10, 12, 6}, l1{192u << 10, 8, 6}, l2{3u << 20, 12, 6}, l3{30u << 20, 12, 6};
    Cache stlb{2048u * 4096u, 16, 12};                // 2048 entries of 4 KB pages
    long long m0 = 0, m1 = 0, m2 = 0, m3 = 0, mt = 0;
    // A miss at one level goes on to the next; every level is filled.
    void line(std::uintptr_t a) {
        if (l0.access(a)) return;
        ++m0;
        if (l1.access(a)) return;
        ++m1;
        if (l2.access(a)) return;
        ++m2;
        if (l3.access(a)) return;
        ++m3;
    }
    void page(std::uintptr_t a) { if (!stlb.access(a)) ++mt; }
    void reset_counts() { m0 = m1 = m2 = m3 = mt = 0; }
};

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
static void run(const char* name, const std::vector<u64>& keys, const std::vector<u64>& queries) {
    S st;
    st.build(keys);
    Hierarchy h;
    std::vector<std::uintptr_t> lines, pages;
    long long total_lines = 0, total_bytes = 0, total_levels = 0;
    std::map<int, long long> level_hist;

    for (int pass = 0; pass < 2; ++pass) {            // pass 0 is warm-up
        if (pass == 1) h.reset_counts();
        for (u64 q : queries) {
            lines.clear(); pages.clear();
            long long bytes = 0;
            st.touch(q, [&](const void* p, std::size_t len) {
                auto a = (std::uintptr_t)p;
                bytes += (long long)len;
                for (std::uintptr_t l = a >> 6; l <= (a + len - 1) >> 6; ++l) lines.push_back(l << 6);
                for (std::uintptr_t g = a >> 12; g <= (a + len - 1) >> 12; ++g) pages.push_back(g << 12);
            });
            // Distinct lines and pages, in first-touch order.
            std::vector<std::uintptr_t> seen;
            for (auto l : lines) if (std::find(seen.begin(), seen.end(), l) == seen.end()) seen.push_back(l);
            for (auto l : seen) h.line(l);
            std::vector<std::uintptr_t> pseen;
            for (auto g : pages) if (std::find(pseen.begin(), pseen.end(), g) == pseen.end()) pseen.push_back(g);
            for (auto g : pseen) h.page(g);
            if (pass == 1) {
                total_lines += (long long)seen.size();
                total_bytes += bytes;
                int lv = st.levels(q);
                total_levels += lv;
                ++level_hist[lv];
            }
        }
    }
    double nq = (double)queries.size();
    long long modal = 0;
    for (auto& [lv, c] : level_hist) modal = std::max(modal, c);
    std::printf("%s,%zu,%.3f,%.4f,%.2f,%.1f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                name, keys.size(), total_levels / nq, modal / nq,
                total_lines / nq, total_bytes / nq,
                h.m0 / nq, h.m1 / nq, h.m2 / nq, h.m3 / nq, h.mt / nq);
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    int max_log = (argc > 1) ? std::atoi(argv[1]) : 22;
    std::mt19937_64 rng(20260909);
    std::size_t nq = 1u << 16;
    std::printf("structure,n,levels_mean,levels_modal_share,lines_per_query,bytes_per_query,"
                "l0_mpq,l1_mpq,l2_mpq,l3_mpq,stlb_mpq\n");
    for (int lg = 8; lg <= max_log; ++lg) {
        std::size_t n = (std::size_t)1 << lg;
        std::vector<u64> keys = make_sorted_keys(n, rng);
        std::vector<u64> queries(nq);
        for (auto& q : queries) q = rng();
        run<SortedArray>("sorted_array", keys, queries);
        run<BTree8>("btree8", keys, queries);
        run<BTree8BL>("btree8_bl", keys, queries);
        run<BTree8A64>("btree8_a64", keys, queries);
        run<BTree8BLA64>("btree8_bl_a64", keys, queries);
        run<BTree16BLA64>("btree16_bl_a64", keys, queries);
        run<Fusion8>("fusion8", keys, queries);
        run<Fusion8BF>("fusion8_bf", keys, queries);
        run<Fusion8C>("fusion8_c", keys, queries);
        run<Fusion16W>("fusion16_w", keys, queries);
    }
    return 0;
}
