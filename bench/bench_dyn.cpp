// bench_dyn.cpp — the changing-data race: inserts mixed with searches.
//
// For one key set of N distinct keys:
//   1. Bulk-load a random half (not timed).
//   2. Run N/2 operations, each an insert (of a key from the other half) or a
//      predecessor search, in a fixed random order. write_pct sets the share of
//      inserts. This part is timed.
// Every structure runs the identical operation sequence, and every search
// result is folded into a checksum, so all of them must print the same one.
//
// Compiled as C++17: ALEX uses std::allocator::rebind, removed in C++20.
//
// Run: ./build/bench_dyn [reps=5] [cpu=-1] [write_pct=10] [structures=all|a,b]
//                        [seed=20260909] [uniform|clustered|nested|file:<path>] [log_n=24]
// Output: CSV on stdout: structure,n,write_pct,rep,ns_per_op,checksum,bytes_end
#include "cluster_jump_dynamic.hpp"
#include "dynamic_rivals.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

using namespace ft;

static volatile u64 g_sink;

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

// Key shapes, as in bench_pred.cpp.
static u64 draw(std::mt19937_64& rng, const u64* centre, const u64* sub) {
    u64 x = rng();
    if (sub) return centre[x & 63] | sub[x & 4095] | (x >> 44);
    return centre ? (centre[x & 63] | (x >> 32)) : x;
}

static std::vector<u64> load_sosd(const char* path) {
    std::vector<u64> v;
    FILE* f = std::fopen(path, "rb");
    if (!f) return v;
    u64 count = 0;
    if (std::fread(&count, sizeof count, 1, f) == 1) {
        v.resize(count);
        v.resize(std::fread(v.data(), sizeof(u64), count, f));
    }
    std::fclose(f);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

struct Workload {
    std::vector<u64> initial;          // sorted
    std::vector<u64> vals;             // insert key or search query
    std::vector<std::uint8_t> insert;  // 1 = insert
    std::vector<u64> warm;             // searches run before timing
};

template <typename S>
static void run_one(const char* name, const Workload& w, std::size_t n, int write_pct, int rep) {
    S s;
    s.bulk_load(w.initial);
    u64 sink = 0;
    for (u64 q : w.warm) { u64 o = 0; if (s.predecessor(q, o)) sink ^= o; }
    g_sink = sink;

    const std::size_t m = w.vals.size();
    u64 cs = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < m; ++i) {
        if (w.insert[i]) {
            s.insert(w.vals[i]);
        } else {
            u64 o = 0;
            if (s.predecessor(w.vals[i], o)) cs ^= o;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / (double)m;
    std::printf("%s,%zu,%d,%d,%.4f,%llu,%zu\n", name, n, write_pct, rep, ns, (unsigned long long)cs, s.bytes());
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    int reps = (argc > 1) ? std::atoi(argv[1]) : 5;
    int cpu = (argc > 2) ? std::atoi(argv[2]) : -1;
    int write_pct = (argc > 3) ? std::atoi(argv[3]) : 10;
    std::string only = (argc > 4) ? argv[4] : "all";
    unsigned long long seed = (argc > 5) ? std::strtoull(argv[5], nullptr, 10) : 20260909ull;
    const char* shape = (argc > 6) ? argv[6] : "uniform";
    int log_n = (argc > 7) ? std::atoi(argv[7]) : 24;
    std::size_t n = std::size_t(1) << log_n;

    struct Entry { const char* name; std::function<void(const Workload&, int)> run; };
    std::vector<Entry> all = {
        {"clusterjump_d", [&](const Workload& w, int rep) { run_one<ClusterJumpD>("clusterjump_d", w, n, write_pct, rep); }},
        {"tlx_btree", [&](const Workload& w, int rep) { run_one<TlxSet>("tlx_btree", w, n, write_pct, rep); }},
        {"alex", [&](const Workload& w, int rep) { run_one<AlexIndex>("alex", w, n, write_pct, rep); }},
    };
    std::vector<Entry> es;
    for (auto& e : all)
        if (only == "all" || ("," + only + ",").find("," + std::string(e.name) + ",") != std::string::npos)
            es.push_back(e);
    if (es.empty()) { std::fprintf(stderr, "no structure matches '%s'\n", only.c_str()); return 2; }

    bool pinned = cpu >= 0 && pin_to_cpu(cpu);
    std::fprintf(stderr, "reps=%d cpu=%d pinned=%d write_pct=%d structures=%zu seed=%llu keys=%s log_n=%d\n",
                 reps, cpu, pinned ? 1 : 0, write_pct, es.size(), seed, shape, log_n);

    std::mt19937_64 rng(seed);
    // All N keys.
    std::vector<u64> keys, dataset;
    u64 centre[64];
    std::vector<u64> sub;
    const u64* cp = nullptr;
    const u64* sp = nullptr;
    bool from_file = std::strncmp(shape, "file:", 5) == 0;
    if (from_file) {
        dataset = load_sosd(shape + 5);
        if (dataset.size() < n) { std::fprintf(stderr, "%s has too few keys\n", shape + 5); return 2; }
    } else if (std::strcmp(shape, "clustered") == 0) {
        for (auto& c : centre) c = rng() & ~0xFFFFFFFFull;
        cp = centre;
    } else if (std::strcmp(shape, "nested") == 0) {
        for (auto& c : centre) c = rng() & ~((1ull << 40) - 1);
        sub.resize(4096);
        for (auto& c : sub) c = rng() & (((1ull << 40) - 1) & ~((1ull << 20) - 1));
        cp = centre; sp = sub.data();
    }
    keys.reserve(n);
    while (keys.size() < n) {
        while (keys.size() < n) keys.push_back(from_file ? dataset[rng() % dataset.size()] : draw(rng, cp, sp));
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    }
    dataset.clear(); dataset.shrink_to_fit();

    // Split: a random half is bulk-loaded, the other half is inserted.
    std::vector<u64> perm = keys;
    std::shuffle(perm.begin(), perm.end(), rng);
    Workload w;
    w.initial.assign(perm.begin(), perm.begin() + n / 2);
    std::sort(w.initial.begin(), w.initial.end());

    auto query = [&]() -> u64 {
        if (!from_file) return draw(rng, cp, sp);
        std::size_t i = rng() % n;                        // a random point in a random gap
        u64 gap = i + 1 < n ? keys[i + 1] - keys[i] : 1;
        return keys[i] + rng() % gap;
    };

    const std::size_t m = n / 2;
    const std::size_t inserts = m * (std::size_t)write_pct / 100;
    w.insert.assign(m, 0);
    std::fill(w.insert.begin(), w.insert.begin() + inserts, 1);
    std::shuffle(w.insert.begin(), w.insert.end(), rng);
    w.vals.resize(m);
    std::size_t next = n / 2;
    for (std::size_t i = 0; i < m; ++i) w.vals[i] = w.insert[i] ? perm[next++] : query();
    w.warm.resize(1u << 16);
    for (auto& q : w.warm) q = query();
    perm.clear(); perm.shrink_to_fit();

    std::printf("structure,n,write_pct,rep,ns_per_op,checksum,bytes_end\n");
    std::mt19937 order_rng(12345);
    std::vector<int> order(es.size());
    for (std::size_t i = 0; i < es.size(); ++i) order[i] = (int)i;
    for (int rep = 0; rep < reps; ++rep) {
        std::shuffle(order.begin(), order.end(), order_rng);
        for (int which : order) es[which].run(w, rep);
    }
    return 0;
}
