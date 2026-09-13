// index_bytes.cpp — how much memory each structure uses, keys included.
//
// For each dataset file given, samples 2^25 distinct keys (seed 20260910, the
// first confirmation seed), builds every structure, and prints bytes per key.
// A plain sorted array of 64-bit keys is 8.00 bytes per key; anything above
// that is the price of the index.
//
// Run: ./build/index_bytes data/books_200M_uint64 data/wiki_ts_200M_uint64 ...
// Output: CSV on stdout: dataset,structure,bytes,bytes_per_key
#include "splus_tree.hpp"
#include "cluster_jump.hpp"
#include "learned_indexes.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace ft;

static std::vector<u64> load(const char* path) {
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

template <typename S>
static void report(const char* dataset, const char* name, const std::vector<u64>& keys) {
    S s;
    s.build(keys);
    std::printf("%s,%s,%zu,%.2f\n", dataset, name, s.bytes(), (double)s.bytes() / keys.size());
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    std::printf("dataset,structure,bytes,bytes_per_key\n");
    for (int i = 1; i < argc; ++i) {
        std::vector<u64> data = load(argv[i]);
        if (data.size() < (1u << 25)) { std::fprintf(stderr, "skipping %s: too few keys\n", argv[i]); continue; }
        std::mt19937_64 rng(20260910);
        std::vector<u64> keys;
        const std::size_t n = std::size_t(1) << 25;
        while (keys.size() < n) {
            while (keys.size() < n) keys.push_back(data[rng() % data.size()]);
            std::sort(keys.begin(), keys.end());
            keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        }
        std::string name = argv[i];
        name = name.substr(name.find_last_of("/\\") + 1);
        const char* ds = name.c_str();
        report<SPlus8>(ds, "splus8", keys);
        report<SPlus16>(ds, "splus16", keys);
        report<ClusterJump>(ds, "clusterjump", keys);
        report<RS18E8>(ds, "rs18_e8", keys);
        report<RS18E16>(ds, "rs18_e16", keys);
        report<RS18E32>(ds, "rs18_e32", keys);
        report<RS22E8>(ds, "rs22_e8", keys);
        report<RS22E16>(ds, "rs22_e16", keys);
        report<RS22E32>(ds, "rs22_e32", keys);
        report<PGM16>(ds, "pgm16", keys);
        report<PGM32>(ds, "pgm32", keys);
        report<PGM64>(ds, "pgm64", keys);
        report<PGM128>(ds, "pgm128", keys);
    }
    return 0;
}
