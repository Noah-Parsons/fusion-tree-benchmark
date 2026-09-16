// bench_one.cpp — a light benchmark harness: one size, one structure in memory at a time.
//
// Why it exists. bench_pred keeps every structure being raced alive at once,
// at every size from 2^8 to 2^25, next to the whole 1.6 GB dataset. On
// 2026-09-13 a Stage 1 run of the SOSD-rival race pushed the laptop's memory
// to 100% (and an earlier, heavier run crashed it). This harness measures the
// same thing with far less memory:
//
//   * it reads a 2^25-key sample written by `bench_pred ... dump:`, which is
//     exactly the key set the RMIs were trained on, and races only that size;
//   * in every repetition the structures run in a fresh random order, and
//     each one is BUILT, warmed, timed once and DESTROYED before the next is
//     built, so at most one structure is in memory besides the keys;
//   * it runs under a hard memory cap (a Windows job object); a structure
//     whose build exceeds the cap is reported and skipped;
//   * before every build it checks free physical memory and stops cleanly if
//     it is below a floor; it reports its own peak memory at the end.
//
// Pre-flight (reps = 0): build each listed structure once and report whether
// it FITS under the cap, timing nothing. Run with one structure per process:
// a build that reaches the cap can also be refused the few pages its stack
// needs to grow, which ends the process (seen once: STATUS_STACK_OVERFLOW on
// wiki), so only structures that passed pre-flight go into a timed run.
// The stack is also committed up front at link time (Makefile).
//
// The timed loop (time_once) is copied from bench_pred.cpp unchanged, so the
// two harnesses measure a query the same way.
//
// Run: bench_one <sample file> <tput|lat> <reps> <cpu> <structures>
//                [query_seed=20260913] [min_free_gb=6] [memory_cap_gb=8]
// Output (reps > 0): CSV: structure,n,rep,ns_per_query,checksum,build_ms
// Output (reps = 0): CSV lines: structure,FITS|OVER_MEMORY_CAP,build_ms,peak_gb
#include "splus_tree.hpp"
#include "cluster_jump.hpp"
#include "learned_indexes.hpp"
#include "sosd_rivals.hpp"
#include "rmi_index.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <random>
#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>

using namespace ft;

static volatile u64 g_sink;
static volatile u64 g_zero = 0;

static double free_gb() {
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    return (double)ms.ullAvailPhys / (1024.0 * 1024.0 * 1024.0);
}

// A hard cap on this process's committed memory (a Windows job object). A
// structure whose build needs more than the cap gets std::bad_alloc.
// Checking free memory before a build cannot do this: CHT's builder, for
// example, can grow by many GB during a single build.
static bool cap_memory(double gb) {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    info.ProcessMemoryLimit = (SIZE_T)(gb * 1024.0 * 1024.0 * 1024.0);
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof info)) return false;
    return AssignProcessToJobObject(job, GetCurrentProcess()) != 0;
}

static double peak_gb() {
    PROCESS_MEMORY_COUNTERS pmc;
    K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc);
    return (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0 * 1024.0);
}

static std::vector<u64> load(const char* path) {
    std::vector<u64> v;
    FILE* f = std::fopen(path, "rb");
    if (!f) return v;
    u64 count = 0;
    if (std::fread(&count, 8, 1, f) == 1) { v.resize(count); v.resize(std::fread(v.data(), 8, count, f)); }
    std::fclose(f);
    return v;
}

// Identical to bench_pred.cpp.
template <typename S>
static double time_once(const S& st, const std::vector<u64>& queries, bool latency, u64& checksum) {
    u64 cs = 0;
    auto t0 = std::chrono::steady_clock::now();
    if (!latency) {
        for (u64 q : queries) {
            u64 o = 0;
            if (st.predecessor(q, o)) cs ^= o;
        }
    } else {
        const u64 z = g_zero;
        u64 dep = 0;
        for (u64 q0 : queries) {
            u64 o = 0;
            bool hit = st.predecessor(q0 ^ dep, o);
            if (hit) cs ^= o;
            dep = (o + hit) & z;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    checksum = cs;
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / (double)queries.size();
}

// Build, warm, time once, destroy. Over-cap builds are reported and skipped.
template <typename S>
static void run_one(const char* name, const std::vector<u64>& keys, const std::vector<u64>& queries,
                    bool latency, int rep) {
    try {
        auto b0 = std::chrono::steady_clock::now();
        S s;
        s.build(keys);
        double build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - b0).count();
        u64 sink = 0;
        for (u64 q : queries) { u64 o = 0; if (s.predecessor(q, o)) sink ^= o; }
        g_sink = sink;
        u64 cs = 0;
        double ns = time_once(s, queries, latency, cs);
        std::printf("%s,%zu,%d,%.4f,%llu,%.1f\n", name, keys.size(), rep, ns, (unsigned long long)cs, build_ms);
        std::fflush(stdout);
    } catch (const std::bad_alloc&) {   // everything it allocated is freed as the stack unwinds
        std::fprintf(stderr, "OVER_MEMORY_CAP %s rep=%d\n", name, rep);
    }
}   // s is destroyed here, before the next structure is built

// Pre-flight: build once, return the build time. bad_alloc propagates.
template <typename S>
static double build_only(const std::vector<u64>& keys) {
    auto b0 = std::chrono::steady_clock::now();
    S s;
    s.build(keys);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - b0).count();
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "usage: bench_one <sample> <tput|lat> <reps> <cpu> <structures> [query_seed] [min_free_gb] [memory_cap_gb]\n");
        return 2;
    }
    const char* sample = argv[1];
    bool latency = std::strcmp(argv[2], "lat") == 0;
    int reps = std::atoi(argv[3]);
    int cpu = std::atoi(argv[4]);
    std::string only = argv[5];
    unsigned long long qseed = argc > 6 ? std::strtoull(argv[6], nullptr, 10) : 20260913ull;
    double min_free = argc > 7 ? std::atof(argv[7]) : 6.0;
    double cap = argc > 8 ? std::atof(argv[8]) : 8.0;
    if (!cap_memory(cap)) { std::fprintf(stderr, "could not set the memory cap; refusing to run\n"); return 2; }

    using Run = std::function<void(const std::vector<u64>&, const std::vector<u64>&, bool, int)>;
    using Build = std::function<double(const std::vector<u64>&)>;
    struct Entry { const char* name; Run run; Build build; };
    std::vector<Entry> all;
#define E(T, n) all.push_back({n, \
        [](const std::vector<u64>& k, const std::vector<u64>& q, bool l, int r) { run_one<T>(n, k, q, l, r); }, \
        [](const std::vector<u64>& k) { return build_only<T>(k); }})
    E(SPlus8, "splus8"); E(SPlus16, "splus16"); E(ClusterJump, "clusterjump");
    E(RS18E16, "rs18_e16"); E(PGM16, "pgm16");
    E(CHT64E16, "cht64_e16"); E(CHT64E32, "cht64_e32"); E(CHT64E64, "cht64_e64"); E(CHT64E128, "cht64_e128");
    E(CHT256E16, "cht256_e16"); E(CHT256E32, "cht256_e32"); E(CHT256E64, "cht256_e64"); E(CHT256E128, "cht256_e128");
    E(CHT1024E16, "cht1024_e16"); E(CHT1024E32, "cht1024_e32"); E(CHT1024E64, "cht1024_e64"); E(CHT1024E128, "cht1024_e128");
    E(RMIIndex<1>, "rmi_r1"); E(RMIIndex<2>, "rmi_r2"); E(RMIIndex<3>, "rmi_r3"); E(RMIIndex<4>, "rmi_r4");
    E(RMIIndex<5>, "rmi_r5"); E(RMIIndex<6>, "rmi_r6"); E(RMIIndex<7>, "rmi_r7"); E(RMIIndex<8>, "rmi_r8");
    E(RMIIndex<9>, "rmi_r9"); E(RMIIndex<10>, "rmi_r10");
#undef E
    std::vector<Entry> es;
    for (auto& e : all)
        if (("," + only + ",").find("," + std::string(e.name) + ",") != std::string::npos) es.push_back(e);
    if (es.empty()) { std::fprintf(stderr, "no structure matches '%s'\n", only.c_str()); return 2; }

    bool pinned = cpu >= 0 && SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(1) << cpu) != 0;
    std::vector<u64> keys = load(sample);
    if (keys.empty()) { std::fprintf(stderr, "cannot read %s\n", sample); return 2; }

    if (reps == 0) {
        for (auto& e : es) {
            std::fprintf(stderr, "START %s preflight\n", e.name);
            std::fflush(stderr);
            double f = free_gb();
            if (f < min_free) { std::fprintf(stderr, "STOP: only %.1f GB free\n", f); return 3; }
            try {
                double ms = e.build(keys);
                std::printf("%s,FITS,%.1f,%.2f\n", e.name, ms, peak_gb());
            } catch (const std::bad_alloc&) {
                std::printf("%s,OVER_MEMORY_CAP,0,%.2f\n", e.name, peak_gb());
            }
            std::fflush(stdout);
        }
        return 0;
    }

    // Queries: a random point in a random gap between stored keys, as in
    // bench_pred's dataset mode.
    std::mt19937_64 rng(qseed);
    std::vector<u64> queries(1u << 16);
    for (auto& q : queries) {
        std::size_t i = rng() % keys.size();
        u64 gap = i + 1 < keys.size() ? keys[i + 1] - keys[i] : 1;
        q = keys[i] + rng() % gap;
    }
    std::fprintf(stderr, "sample=%s mode=%s reps=%d cpu=%d pinned=%d structures=%zu query_seed=%llu keys=%zu free_gb=%.1f cap_gb=%.1f\n",
                 sample, latency ? "lat" : "tput", reps, cpu, pinned ? 1 : 0, es.size(), qseed, keys.size(), free_gb(), cap);

    std::printf("structure,n,rep,ns_per_query,checksum,build_ms\n");
    std::mt19937 order_rng(12345);
    std::vector<int> order(es.size());
    for (std::size_t i = 0; i < es.size(); ++i) order[i] = (int)i;
    for (int rep = 0; rep < reps; ++rep) {
        std::shuffle(order.begin(), order.end(), order_rng);
        for (int which : order) {
            double f = free_gb();
            if (f < min_free) {
                std::fprintf(stderr, "STOP: only %.1f GB free before %s (floor %.1f GB)\n", f, es[which].name, min_free);
                std::fprintf(stderr, "peak_memory_gb=%.2f\n", peak_gb());
                return 3;
            }
            std::fprintf(stderr, "START %s rep=%d\n", es[which].name, rep);
            std::fflush(stderr);
            es[which].run(keys, queries, latency, rep);
        }
    }
    std::fprintf(stderr, "peak_memory_gb=%.2f\n", peak_gb());
    return 0;
}
