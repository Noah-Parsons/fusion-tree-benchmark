// index_bytes_samples.cpp — memory per key for the SOSD-rival race.
//
// Like index_bytes.cpp, but reads the exact key sets the race used
// (data/samples/<dataset>_<seed>, written by bench_pred ... dump:) so that
// every RMI finds the RMI trained on those keys. Keys are included in every
// figure (a bare array is 8.00 bytes per key). An RMI rank reported at 8.00
// had no matching RMI.
//
// Build with the RMI registry and generated sources linked, like bench_pred.
// Run:   ./build/index_bytes_samples data/samples/books_20260909 ...
// Output: CSV: dataset,structure,bytes,bytes_per_key
#include "splus_tree.hpp"
#include "cluster_jump.hpp"
#include "learned_indexes.hpp"
#include "sosd_rivals.hpp"
#include "rmi_index.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace ft;

static std::vector<u64> load(const char* path) {
    std::vector<u64> v;
    FILE* f = std::fopen(path, "rb");
    if (!f) return v;
    u64 count = 0;
    if (std::fread(&count, 8, 1, f) == 1) { v.resize(count); v.resize(std::fread(v.data(), 8, count, f)); }
    std::fclose(f);
    return v;
}

template <typename S>
static void report(const std::string& ds, const char* name, const std::vector<u64>& keys) {
    S s;
    s.build(keys);
    std::printf("%s,%s,%zu,%.2f\n", ds.c_str(), name, s.bytes(), (double)s.bytes() / keys.size());
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    std::printf("dataset,structure,bytes,bytes_per_key\n");
    for (int i = 1; i < argc; ++i) {
        std::vector<u64> keys = load(argv[i]);
        if (keys.empty()) { std::fprintf(stderr, "cannot read %s\n", argv[i]); continue; }
        std::string ds = argv[i];
        ds = ds.substr(ds.find_last_of("/\\") + 1);
        report<SPlus8>(ds, "splus8", keys);
        report<SPlus16>(ds, "splus16", keys);
        report<ClusterJump>(ds, "clusterjump", keys);
        report<RS18E16>(ds, "rs18_e16", keys);
        report<PGM16>(ds, "pgm16", keys);
        report<CHT64E16>(ds, "cht64_e16", keys);
        report<CHT64E32>(ds, "cht64_e32", keys);
        report<CHT64E64>(ds, "cht64_e64", keys);
        report<CHT64E128>(ds, "cht64_e128", keys);
        report<CHT256E16>(ds, "cht256_e16", keys);
        report<CHT256E32>(ds, "cht256_e32", keys);
        report<CHT256E64>(ds, "cht256_e64", keys);
        report<CHT256E128>(ds, "cht256_e128", keys);
        report<CHT1024E16>(ds, "cht1024_e16", keys);
        report<CHT1024E32>(ds, "cht1024_e32", keys);
        report<CHT1024E64>(ds, "cht1024_e64", keys);
        report<CHT1024E128>(ds, "cht1024_e128", keys);
        report<RMIIndex<1>>(ds, "rmi_r1", keys);
        report<RMIIndex<2>>(ds, "rmi_r2", keys);
        report<RMIIndex<3>>(ds, "rmi_r3", keys);
        report<RMIIndex<4>>(ds, "rmi_r4", keys);
        report<RMIIndex<5>>(ds, "rmi_r5", keys);
        report<RMIIndex<6>>(ds, "rmi_r6", keys);
        report<RMIIndex<7>>(ds, "rmi_r7", keys);
        report<RMIIndex<8>>(ds, "rmi_r8", keys);
        report<RMIIndex<9>>(ds, "rmi_r9", keys);
        report<RMIIndex<10>>(ds, "rmi_r10", keys);
    }
    return 0;
}
