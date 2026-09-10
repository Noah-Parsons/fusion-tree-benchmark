// test_node.cpp — differential test of every fusion node type against brute force.
//
// The oracle is a linear scan. It is obviously correct, which is exactly what
// an oracle needs to be. Every disagreement is printed with the inputs that
// caused it, so a failure is immediately reproducible.
//
// Node types tested, each on the same key sets (same seed):
//   FusionNode<8>        the original rank
//   FusionNode<8, true>  branch-free rank
//   FusionNodeCompact    128-byte-tree layout, branch-free rank
//   FusionNodeWide       K = 16 on a 256-bit word (key sets of up to 16)
//
// Build:  make test
#include "fusion_node.hpp"
#include "fusion_node_compact.hpp"
#include "fusion_node_wide.hpp"
#include <cstdio>
#include <random>
#include <set>
#include <vector>

using namespace ft;

static int brute_rank(const std::vector<u64>& ks, u64 q) {
    int c = 0;
    for (u64 k : ks) if (k < q) ++c;
    return c;
}

// Draw n distinct keys. `mode` controls how adversarial the key set is:
//   0 = uniform over the whole 64-bit range
//   1 = keys sharing a long common prefix (stresses the important-bit logic)
//   2 = keys drawn from a small range (many shared high bits)
//   3 = keys differing only in scattered high bits
static std::vector<u64> make_keys(std::mt19937_64& rng, int n, int mode) {
    std::set<u64> s;
    while ((int)s.size() < n) {
        u64 x = rng();
        if (mode == 1) x = (0xABCDEF0000000000ull) | (x & 0xFFFFFFull);
        if (mode == 2) x = x % 1024;
        if (mode == 3) x = (x & 0xF0F0F0F0F0F0F0F0ull);
        s.insert(x);
    }
    return std::vector<u64>(s.begin(), s.end());
}

template <typename Node, int MaxN>
static long long run(const char* label, long long& total_checks) {
    std::mt19937_64 rng(20260909);
    long long checks = 0, failures = 0;

    for (int mode = 0; mode < 4; ++mode) {
        for (int trial = 0; trial < 20000; ++trial) {
            int n = 1 + (int)(rng() % MaxN);
            std::vector<u64> ks = make_keys(rng, n, mode);

            Node node;
            node.build(ks.data(), n);

            // Queries: every stored key, each stored key +/- 1, and randoms.
            std::vector<u64> qs;
            for (u64 k : ks) {
                qs.push_back(k);
                if (k > 0) qs.push_back(k - 1);
                if (k < ~0ull) qs.push_back(k + 1);
            }
            for (int i = 0; i < 8; ++i) qs.push_back(make_keys(rng, 1, mode)[0]);
            qs.push_back(0);
            qs.push_back(~0ull);

            for (u64 q : qs) {
                int got = node.rank(q);
                int want = brute_rank(ks, q);
                ++checks;
                if (got != want) {
                    ++failures;
                    if (failures <= 10) {
                        std::printf("MISMATCH %s mode=%d n=%d q=%llu got=%d want=%d\n keys:",
                                    label, mode, n, (unsigned long long)q, got, want);
                        for (u64 k : ks) std::printf(" %llu", (unsigned long long)k);
                        std::printf("\n");
                    }
                }
            }
        }
    }
    std::printf("  %-20s checks=%lld failures=%lld\n", label, checks, failures);
    total_checks += checks;
    return failures;
}

int main() {
    long long checks = 0, failures = 0;
    failures += run<FusionNode<8>, 8>("FusionNode<8>", checks);
    failures += run<FusionNode<8, true>, 8>("FusionNode<8,bf>", checks);
    failures += run<FusionNodeCompact, 8>("FusionNodeCompact", checks);
    failures += run<FusionNodeWide, 16>("FusionNodeWide", checks);
    std::printf("checks=%lld failures=%lld\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
