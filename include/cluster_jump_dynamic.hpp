// cluster_jump_dynamic.hpp — ClusterJump for data that changes (insertions).
//
// The static ClusterJump keeps every key in one sorted array, so adding a key
// would mean shifting millions of others. This version keeps the same two
// radix levels but gives every second-level slot its own small box of keys.
//
//   TOP.     q's top bits pick a bucket (at most 2^16, fixed at bulk load).
//   SLOT.    Each bucket is rescaled to its own keys, as in ClusterJump, and
//            q's slot IS a box: box j starts at blocks + j * 16. No position
//            table is read, so a search usually waits on memory once.
//   BOX.     16 keys, sorted, empty places filled with a key that is never
//            < q. One AVX2 pass counts the keys below q. If there are none,
//            the answer is the box's "floor": the largest key before it.
//
// INSERT. Put the key in its box (shifting at most 15 others) and raise the
// floors after it. A full box spills into an overflow list. When overflow
// grows past 1/8 of a bucket since its last rebuild, the bucket is rebuilt:
// rescaled to its keys, about 6 keys per box, so there is room again.
//
// What it does not do: deletion, and clumps inside clumps. A sub-cluster
// narrower than one box ends up in an overflow list, searched by halving.
#pragma once
#include "bits.hpp"
#include <immintrin.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ft {

class ClusterJumpD {
    static constexpr u64 FLIP = 0x8000000000000000ull;
    static constexpr u64 PAD = ~0ull ^ FLIP;           // never < q
    static constexpr std::size_t CAP = 16;             // keys per box
    static constexpr std::size_t TARGET = 6;           // keys per box after a rebuild

    struct Bucket {
        u64 base = 0;
        std::uint32_t shift = 63, slots = 0;
        u64* blocks = nullptr;                // slots × CAP flipped keys
        u64* floor = nullptr;                 // largest key before box j (plain)
        std::uint8_t* has = nullptr;          // floor[j] exists
        std::uint32_t* count = nullptr;       // keys in box j
        std::uint32_t* over = nullptr;        // 0, or 1 + index into big
        std::vector<std::vector<u64>> big;    // overflowed boxes: flipped, sorted, 8 PAD after
        std::size_t total = 0, overflow_keys = 0, overflow_at_rebuild = 0;
        u64 bfloor = 0;                       // for a bucket with no boxes
        bool bhas = false;
    };

public:
    ClusterJumpD() = default;
    ClusterJumpD(const ClusterJumpD&) = delete;
    ClusterJumpD& operator=(const ClusterJumpD&) = delete;
    ~ClusterJumpD() { for (auto& b : bk_) release(b); }

    // Sorted, distinct keys.
    void bulk_load(const std::vector<u64>& a) {
        for (auto& b : bk_) release(b);
        bk_.clear();
        n_ = a.size();
        if (a.empty()) { nb_ = 0; return; }
        gbase_ = a.front();
        int lgn = 63 - __builtin_clzll(n_);
        int tbits = lgn - 6 < 1 ? 1 : (lgn - 6 > 16 ? 16 : lgn - 6);
        gshift_ = shift_for(a.back() - gbase_, tbits);
        nb_ = ((a.back() - gbase_) >> gshift_) + 1;
        bk_.resize(nb_);
        std::size_t i = 0;
        std::vector<u64> keys;
        for (std::size_t b = 0; b < nb_; ++b) {
            std::size_t s = i;
            while (i < n_ && ((a[i] - gbase_) >> gshift_) == b) ++i;
            keys.assign(a.begin() + s, a.begin() + i);
            rebuild(b, keys);
        }
        // Floors, in one pass over everything.
        u64 run = 0;
        bool rh = false;
        for (auto& B : bk_) {
            if (B.slots == 0) { B.bfloor = run; B.bhas = rh; continue; }
            for (std::uint32_t j = 0; j < B.slots; ++j) {
                B.floor[j] = run; B.has[j] = rh;
                if (B.count[j]) { run = last_key(B, j); rh = true; }
            }
        }
    }
    void build(const std::vector<u64>& a) { bulk_load(a); }

    bool predecessor(u64 q, u64& out) const {
        if (nb_ == 0) return false;
        const Bucket& B = bk_[bucket_of(q)];
        if (B.slots == 0) { out = B.bfloor; return B.bhas; }
        std::uint32_t j = slot_of(B, q);
        if (B.overflow_keys && B.over[j]) {
            const std::vector<u64>& v = B.big[B.over[j] - 1];
            std::size_t r = rank_sorted(v.data(), B.count[j], q);
            if (r) { out = v[r - 1] ^ FLIP; return true; }
        } else {
            const u64* box = B.blocks + std::size_t(j) * CAP;
            std::size_t r = rank16(box, q);
            if (r) { out = box[r - 1] ^ FLIP; return true; }
        }
        out = B.floor[j];
        return B.has[j];
    }

    void insert(u64 k) {
        if (nb_ == 0) { bulk_load(std::vector<u64>{k}); return; }
        std::size_t b = bucket_of(k);
        Bucket& B = bk_[b];
        if (B.slots == 0) {
            std::vector<u64> keys{k};
            rebuild(b, keys);
            ++n_;
            raise_floors(b, B.slots - 1, k);
            return;
        }
        std::uint32_t j = slot_of(B, k);
        if (B.over[j]) {
            std::vector<u64>& v = B.big[B.over[j] - 1];
            std::size_t pos = rank_sorted(v.data(), B.count[j], k);
            if (pos < B.count[j] && v[pos] == (k ^ FLIP)) return;       // already present
            v.insert(v.begin() + pos, k ^ FLIP);
            ++B.count[j]; ++B.overflow_keys;
        } else {
            u64* box = B.blocks + std::size_t(j) * CAP;
            std::size_t pos = rank16(box, k);
            if (pos < B.count[j] && box[pos] == (k ^ FLIP)) return;     // already present
            if (B.count[j] == CAP) {
                // Full: spill the box into an overflow list.
                std::vector<u64> v(box, box + CAP);
                v.insert(v.begin() + pos, k ^ FLIP);
                v.insert(v.end(), 8, PAD);
                B.big.push_back(std::move(v));
                B.over[j] = (std::uint32_t)B.big.size();
                for (std::size_t i = 0; i < CAP; ++i) box[i] = PAD;
                B.count[j] = CAP + 1;
                B.overflow_keys += CAP + 1;
            } else {
                std::memmove(box + pos + 1, box + pos, (B.count[j] - pos) * sizeof(u64));
                box[pos] = k ^ FLIP;
                ++B.count[j];
            }
        }
        ++B.total; ++n_;
        raise_floors(b, j, k);
        if (B.total > 64 && (B.overflow_keys - B.overflow_at_rebuild) * 8 > B.total) {
            std::vector<u64> keys = collect(B);
            rebuild(b, keys);
        }
    }

    std::size_t size() const { return n_; }
    std::size_t bytes() const {
        std::size_t s = bk_.capacity() * sizeof(Bucket);
        for (const auto& B : bk_) {
            s += std::size_t(B.slots) * (CAP * sizeof(u64) + sizeof(u64) + 1 + 2 * sizeof(std::uint32_t));
            for (const auto& v : B.big) s += v.capacity() * sizeof(u64);
        }
        return s;
    }
    int levels(u64) const { return 2; }

    // Diagnostics only (not used by any search or insert).
    struct Stats {
        std::size_t buckets = 0, nonempty_buckets = 0, largest_bucket = 0;
        std::size_t overflow_lists = 0, overflow_keys = 0, largest_overflow = 0;
    };
    Stats stats() const {
        Stats s;
        s.buckets = nb_;
        for (const auto& B : bk_) {
            if (B.total) ++s.nonempty_buckets;
            if (B.total > s.largest_bucket) s.largest_bucket = B.total;
            s.overflow_keys += B.overflow_keys;
            for (std::uint32_t j = 0; j < B.slots; ++j)
                if (B.over[j]) {
                    ++s.overflow_lists;
                    if (B.count[j] > s.largest_overflow) s.largest_overflow = B.count[j];
                }
        }
        return s;
    }

private:
    static int shift_for(u64 span, int bits) {
        int width = span ? 64 - __builtin_clzll(span) : 0;
        return width > bits ? width - bits : 0;
    }
    std::size_t bucket_of(u64 k) const {
        u64 b = (k - gbase_) >> gshift_;
        b = k < gbase_ ? 0 : b;
        return b < nb_ ? (std::size_t)b : nb_ - 1;
    }
    static std::uint32_t slot_of(const Bucket& B, u64 k) {
        u64 j = (k - B.base) >> B.shift;
        j = k < B.base ? 0 : j;
        return j < B.slots ? (std::uint32_t)j : B.slots - 1;
    }
    // Keys below q in a 16-key box (empty places are PAD, never counted).
    static std::size_t rank16(const u64* box, u64 q) {
        const __m256i qv = _mm256_set1_epi64x((long long)(q ^ FLIP));
        unsigned m = 0;
        for (int i = 0; i < 16; i += 4) {
            __m256i k = _mm256_load_si256(reinterpret_cast<const __m256i*>(box + i));
            m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))) << i;
        }
        return (std::size_t)popcount(m);
    }
    // Keys below q among the first len of a sorted, PAD-followed list.
    static std::size_t rank_sorted(const u64* keys, std::size_t len, u64 q) {
        const long long qs = (long long)(q ^ FLIP);
        std::size_t lo = 0;
        while (len > 8) {
            std::size_t half = len / 2;
            lo += ((long long)keys[lo + half] < qs) ? (len - half) : 0;
            len = half;
        }
        const __m256i qv = _mm256_set1_epi64x(qs);
        unsigned m = 0;
        for (int i = 0; i < 8; i += 4) {
            __m256i k = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys + lo + i));
            m |= (unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(qv, k))) << i;
        }
        return lo + (std::size_t)popcount(m & ((1u << len) - 1));
    }
    static u64 last_key(const Bucket& B, std::uint32_t j) {
        return (B.over[j] ? B.big[B.over[j] - 1][B.count[j] - 1] : B.blocks[std::size_t(j) * CAP + B.count[j] - 1]) ^ FLIP;
    }

    // k was just added in box j of bucket b: every floor after it, up to and
    // including the next box that holds keys, must be at least k.
    void raise_floors(std::size_t b, std::uint32_t j, u64 k) {
        Bucket& B = bk_[b];
        for (std::uint32_t jj = j + 1; jj < B.slots; ++jj) {
            if (B.has[jj] && B.floor[jj] >= k) return;
            B.floor[jj] = k; B.has[jj] = 1;
            if (B.count[jj]) return;
        }
        for (std::size_t bb = b + 1; bb < nb_; ++bb) {
            Bucket& C = bk_[bb];
            if (C.slots == 0) {
                if (C.bhas && C.bfloor >= k) return;
                C.bfloor = k; C.bhas = true;
                continue;
            }
            for (std::uint32_t jj = 0; jj < C.slots; ++jj) {
                if (C.has[jj] && C.floor[jj] >= k) return;
                C.floor[jj] = k; C.has[jj] = 1;
                if (C.count[jj]) return;
            }
        }
    }

    static std::vector<u64> collect(const Bucket& B) {
        std::vector<u64> out;
        out.reserve(B.total);
        for (std::uint32_t j = 0; j < B.slots; ++j) {
            const u64* p = B.over[j] ? B.big[B.over[j] - 1].data() : B.blocks + std::size_t(j) * CAP;
            for (std::uint32_t i = 0; i < B.count[j]; ++i) out.push_back(p[i] ^ FLIP);
        }
        return out;
    }

    // Rebuild bucket b from its sorted keys: rescale, about TARGET per box.
    void rebuild(std::size_t b, const std::vector<u64>& keys) {
        Bucket& B = bk_[b];
        u64 before = B.slots ? B.floor[0] : B.bfloor;
        bool bh = B.slots ? (bool)B.has[0] : B.bhas;
        release(B);
        B.total = keys.size();
        if (keys.empty()) { B.bfloor = before; B.bhas = bh; return; }
        B.base = keys.front();
        int lgs = 0;
        while ((std::size_t(1) << (lgs + 1)) <= keys.size() / TARGET && lgs < 30) ++lgs;
        B.shift = (std::uint32_t)shift_for(keys.back() - B.base, lgs);
        B.slots = (std::uint32_t)(((keys.back() - B.base) >> B.shift) + 1);
        std::size_t S = B.slots;
        B.blocks = static_cast<u64*>(_mm_malloc(S * CAP * sizeof(u64), 64));
        for (std::size_t i = 0; i < S * CAP; ++i) B.blocks[i] = PAD;
        B.floor = new u64[S];
        B.has = new std::uint8_t[S];
        B.count = new std::uint32_t[S]();
        B.over = new std::uint32_t[S]();
        u64 run = before;
        bool rh = bh;
        std::size_t i = 0;
        for (std::uint32_t j = 0; j < B.slots; ++j) {
            B.floor[j] = run; B.has[j] = rh;
            std::size_t s = i;
            while (i < keys.size() && slot_of(B, keys[i]) == j) ++i;
            std::size_t c = i - s;
            B.count[j] = (std::uint32_t)c;
            if (c > CAP) {
                std::vector<u64> v;
                v.reserve(c + 8);
                for (std::size_t t = s; t < i; ++t) v.push_back(keys[t] ^ FLIP);
                v.insert(v.end(), 8, PAD);
                B.big.push_back(std::move(v));
                B.over[j] = (std::uint32_t)B.big.size();
                B.overflow_keys += c;
            } else {
                for (std::size_t t = s; t < i; ++t) B.blocks[std::size_t(j) * CAP + (t - s)] = keys[t] ^ FLIP;
            }
            if (c) { run = keys[i - 1]; rh = true; }
        }
        B.overflow_at_rebuild = B.overflow_keys;
    }

    static void release(Bucket& B) {
        if (B.blocks) _mm_free(B.blocks);
        delete[] B.floor; delete[] B.has; delete[] B.count; delete[] B.over;
        B.blocks = nullptr; B.floor = nullptr; B.has = nullptr; B.count = nullptr; B.over = nullptr;
        B.big.clear(); B.big.shrink_to_fit();
        B.slots = 0; B.total = 0; B.overflow_keys = 0; B.overflow_at_rebuild = 0;
    }

    std::vector<Bucket> bk_;
    std::size_t n_ = 0, nb_ = 0;
    u64 gbase_ = 0;
    int gshift_ = 0;
};

} // namespace ft
