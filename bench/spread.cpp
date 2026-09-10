// spread.cpp — Experiment 1: how wide is the sketch window in practice?
//
// The fusion tree's branching factor K is limited by how many bits a sketch
// needs. The theory says the multiplication trick lands the r important bits
// inside a window of width at most r^4. For r = 7 that bound is 2401 bits,
// which is 37 times wider than a machine word — if the worst case were the
// typical case, the classical construction could not be built at all at
// W = 64.
//
// This program measures the window width actually achieved on random key
// sets, so the project can say something quantitative rather than repeating
// the worst-case bound. It runs node sizes K = 2..16, so the same question
// can be asked of a 256-bit word (Experiment 4).
//
// Output: CSV on stdout. Columns:
//   k, r, spread, bound_r4, fields_fit, fields_fit256
//
// fields_fit is the question that actually matters: can K sketch fields, each
// one bit wider than the sketch itself, be packed into a single 64-bit word?
// fields_fit256 asks the same of a 256-bit word. If not, the classical
// multiplication-based fusion node cannot be built at that word width,
// whatever the asymptotics say.
//
// Build: make spread
#include "sketch_fast.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <vector>

using namespace ft;

int main(int argc, char** argv) {
    int trials = (argc > 1) ? std::atoi(argv[1]) : 2000;
    std::mt19937_64 rng(20260909);

    std::printf("k,r,spread,bound_r4,fields_fit,fields_fit256\n");

    for (int k = 2; k <= 16; ++k) {
        for (int t = 0; t < trials; ++t) {
            std::set<u64> s;
            while ((int)s.size() < k) s.insert(rng());
            std::vector<u64> keys(s.begin(), s.end());

            // Important bits: the highest differing bit of each adjacent pair.
            std::vector<int> pos;
            for (int i = 0; i + 1 < k; ++i) pos.push_back(highest_diff_bit(keys[i], keys[i + 1]));
            std::sort(pos.begin(), pos.end());
            pos.erase(std::unique(pos.begin(), pos.end()), pos.end());
            int r = (int)pos.size();
            if (r == 0) continue;

            Multiplier mul = find_multiplier(pos.data(), r);

            long long bound = 1;
            for (int i = 0; i < 4; ++i) bound *= r;

            int field = mul.spread() + 1;
            std::printf("%d,%d,%d,%lld,%d,%d\n", k, r, mul.spread(), bound,
                        (k * field <= 64) ? 1 : 0, (k * field <= 256) ? 1 : 0);
        }
    }
    return 0;
}
