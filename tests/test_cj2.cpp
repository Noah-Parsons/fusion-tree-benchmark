// test_cj2.cpp — differential test of ClusterJump2 (and ClusterJump) against std::lower_bound.
//
// Key sets: uniform, clustered, nested clusters, outliers above / below / both, a single huge
// outlier sharing the bulk's top bucket (the fb failure), tiny sets, and the real fb tuning
// sample if present. Queries: every kind of boundary (keys, keys +-1, 0, max, between buckets)
// plus random values. Any mismatch fails the run.
#include "cluster_jump.hpp"
#include "cluster_jump2.hpp"
#include <algorithm>
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace ft;

static bool oracle(const std::vector<u64>& a, u64 q, u64& out) {
    auto it = std::lower_bound(a.begin(), a.end(), q);
    if (it == a.begin()) return false;
    out = *(it - 1);
    return true;
}

static long long checks = 0, failures = 0;

template <typename S>
static void run(const char* name, const std::string& set, const std::vector<u64>& a, std::mt19937_64& rng) {
    S s;
    s.build(a);
    std::vector<u64> qs = {0, 1, ~0ull, ~0ull - 1};
    std::size_t step = a.size() > 200000 ? a.size() / 200000 : 1;
    for (std::size_t i = 0; i < a.size(); i += step) { qs.push_back(a[i]); qs.push_back(a[i] + 1); qs.push_back(a[i] - 1); }
    if (!a.empty()) { qs.push_back(a.back()); qs.push_back(a.back() + 1); qs.push_back(a.front()); qs.push_back(a.front() - 1); }
    for (int i = 0; i < 200000; ++i) qs.push_back(rng());
    for (int i = 0; i < 200000 && !a.empty(); ++i) {           // near the bulk
        u64 lo = a.front(), hi = a[a.size() * 9 / 10];
        qs.push_back(lo + (hi > lo ? rng() % (hi - lo + 1) : 0));
    }
    long long bad = 0;
    for (u64 q : qs) {
        u64 w = 0, g = 0;
        bool hw = oracle(a, q, w), h = s.predecessor(q, g);
        ++checks;
        if (h != hw || (hw && g != w)) {
            if (++bad <= 3) std::printf("  MISMATCH %s on %s: q=%llu got=%llu want=%llu\n", name, set.c_str(),
                                        (unsigned long long)q, (unsigned long long)g, (unsigned long long)w);
        }
    }
    failures += bad;
}

static std::vector<u64> finish(std::set<u64>& s) { return {s.begin(), s.end()}; }

int main(int argc, char** argv) {
    std::mt19937_64 rng(20261019);
    std::vector<std::pair<std::string, std::vector<u64>>> sets;
    auto add = [&](const std::string& n, std::set<u64> s) { sets.push_back({n, finish(s)}); };
    for (std::size_t n : {0u, 1u, 2u, 7u, 33u, 1000u, 200000u}) {
        std::set<u64> s; while (s.size() < n) s.insert(rng()); add("uniform_" + std::to_string(n), s);
    }
    { std::set<u64> s; while (s.size() < 300000) s.insert((rng() % 64) * (1ull << 50) + rng() % 1000000); add("clustered", s); }
    { std::set<u64> s; while (s.size() < 300000) s.insert((rng() % 64) * (1ull << 50) + (rng() % 64) * (1ull << 30) + rng() % 4096); add("nested", s); }
    { std::set<u64> s; while (s.size() < 300000) s.insert(rng() % 100000000000ull); for (int i = 0; i < 7; ++i) s.insert((1ull << 60) + rng() % (1ull << 62)); add("outliers_high", s); }
    { std::set<u64> s; while (s.size() < 300000) s.insert((1ull << 62) + rng() % 100000000000ull); for (int i = 0; i < 9; ++i) s.insert(rng() % 1000); add("outliers_low", s); }
    { std::set<u64> s; while (s.size() < 300000) s.insert((1ull << 40) + rng() % 100000000000ull); for (int i = 0; i < 5; ++i) { s.insert(rng() % 100); s.insert(~0ull - rng() % 100); } add("outliers_both", s); }
    // The fb failure: bulk below 7.7e10, one outlier at 1.4e14 (same top bucket), others far above.
    { std::set<u64> s; while (s.size() < 400000) s.insert(1 + rng() % 77000000000ull);
      s.insert(141000000000000ull); s.insert(704000000000000ull); s.insert(1130000000000000ull);
      s.insert(6150000000000000000ull); s.insert(16400000000000000000ull); add("fb_like", s); }
    { std::set<u64> s; for (u64 i = 0; i < 100000; ++i) s.insert(1000 + i); add("consecutive", s); }
    // The osm failure (2026-10-19): of the keys left out of the fit, some lie just above the
    // bulk (inside the last regular slot) and one far away.
    { std::set<u64> s; while (s.size() < 300000) s.insert((1ull << 40) + rng() % 1000000000ull);
      u64 top = *s.rbegin();
      for (int i = 1; i <= 6; ++i) s.insert(top + i * 37);
      s.insert(top + (1ull << 45)); s.insert(top + (1ull << 46)); add("near_and_far_high", s); }
    { std::set<u64> s; while (s.size() < 300000) s.insert((1ull << 50) + rng() % 1000000000ull);
      u64 bot = *s.begin();
      for (int i = 1; i <= 6; ++i) s.insert(bot - i * 41);
      s.insert(bot - (1ull << 45)); s.insert(bot - (1ull << 46)); add("near_and_far_low", s); }
    for (auto& [n, a] : sets) {
        run<ClusterJump>("clusterjump", n, a, rng);
        run<CJ2_K2W8>("cj2_k2w8", n, a, rng);
        run<CJ2_K3W16>("cj2_k3w16", n, a, rng);
        run<CJ2_K4W16>("cj2_k4w16", n, a, rng);
    }
    sets.clear();
    for (int i = 1; i < argc; ++i) {                           // real samples, one at a time
        FILE* f = std::fopen(argv[i], "rb");
        if (!f) continue;
        u64 c = 0; std::fread(&c, 8, 1, f); std::vector<u64> v(c); v.resize(std::fread(v.data(), 8, c, f)); std::fclose(f);
        std::string n = std::string("sample:") + argv[i];
        run<ClusterJump>("clusterjump", n, v, rng);
        run<CJ2_K2W8>("cj2_k2w8", n, v, rng);
        run<CJ2_K3W16>("cj2_k3w16", n, v, rng);
        run<CJ2_K4W16>("cj2_k4w16", n, v, rng);
    }
    std::printf("%lld checks, %lld failures\n", checks, failures);
    return failures ? 1 : 0;
}
