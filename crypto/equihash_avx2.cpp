// Copyright (c) 2026 The Zakura developers
// Distributed under the MIT license; see LICENSE.
// BLAKE2b and single-pass verification adapted from zakura-equihash 4.0.0,
// crates/equihash/src/{leaf_hash,verify}.rs. No proof-of-work rules change.
#include "equihash_avx2.h"
#include "equihash.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define EQUIHASH_X86_AVX2 1
#define AVX2 __attribute__((target("avx2")))
namespace {
const uint64_t IV[8] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
                        0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
                        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
                        0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};
const unsigned char SIGMA[12][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
    {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
    {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
    {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3}};
uint64_t load64(const uint8_t *p) {
  uint64_t x;
  std::memcpy(&x, p, 8);
  return x;
}
uint64_t rotate(uint64_t x, unsigned n) { return (x >> n) | (x << (64 - n)); }
void mix(uint64_t *v, int a, int b, int c, int d, uint64_t x, uint64_t y) {
  v[a] += v[b] + x;
  v[d] = rotate(v[d] ^ v[a], 32);
  v[c] += v[d];
  v[b] = rotate(v[b] ^ v[c], 24);
  v[a] += v[b] + y;
  v[d] = rotate(v[d] ^ v[a], 16);
  v[c] += v[d];
  v[b] = rotate(v[b] ^ v[c], 63);
}
// Hash the first complete 128-byte header block once per proof. All loads in
// this x86-only section are little endian, as required by BLAKE2b.
void midstate(const uint8_t *header, uint64_t *h) {
  uint8_t param[64] = {};
  param[0] = 50;
  param[2] = 1;
  param[3] = 1;
  std::memcpy(param + 48, "ZcashPoW", 8);
  param[56] = 200;
  param[60] = 9;
  uint64_t v[16], m[16];
  for (int i = 0; i < 8; ++i) {
    h[i] = IV[i] ^ load64(param + 8 * i);
    v[i] = h[i];
    v[i + 8] = IV[i];
  }
  v[12] ^= 128;
  for (int i = 0; i < 16; ++i)
    m[i] = load64(header + 8 * i);
  for (const auto &s : SIGMA) {
    mix(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
    mix(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
    mix(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
    mix(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
    mix(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
    mix(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
    mix(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
    mix(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
  }
  for (int i = 0; i < 8; ++i)
    h[i] ^= v[i] ^ v[i + 8];
}
template <int N> AVX2 inline __m256i ror(__m256i x) {
  return _mm256_or_si256(_mm256_srli_epi64(x, N), _mm256_slli_epi64(x, 64 - N));
}
AVX2 inline void mix4(__m256i *v, int a, int b, int c, int d, __m256i x,
                      __m256i y) {
  v[a] = _mm256_add_epi64(_mm256_add_epi64(v[a], v[b]), x);
  v[d] = ror<32>(_mm256_xor_si256(v[d], v[a]));
  v[c] = _mm256_add_epi64(v[c], v[d]);
  v[b] = ror<24>(_mm256_xor_si256(v[b], v[c]));
  v[a] = _mm256_add_epi64(_mm256_add_epi64(v[a], v[b]), y);
  v[d] = ror<16>(_mm256_xor_si256(v[d], v[a]));
  v[c] = _mm256_add_epi64(v[c], v[d]);
  v[b] = ror<63>(_mm256_xor_si256(v[b], v[c]));
}
// Each lane hashes a distinct leaf group. The final message is 12 header bytes
// followed by a little-endian 32-bit group index, with total input length 144.
AVX2 void hashes4(const uint64_t *h, const uint8_t *tail,
                  const uint32_t *groups, uint8_t out[4][64]) {
  __m256i v[16], m[16];
  for (int i = 0; i < 8; ++i) {
    v[i] = _mm256_set1_epi64x(h[i]);
    v[i + 8] = _mm256_set1_epi64x(IV[i]);
  }
  v[12] = _mm256_xor_si256(v[12], _mm256_set1_epi64x(144));
  v[14] = _mm256_xor_si256(v[14], _mm256_set1_epi64x(-1));
  m[0] = _mm256_set1_epi64x(load64(tail));
  uint32_t low;
  std::memcpy(&low, tail + 8, 4);
  uint64_t words[4];
  for (int i = 0; i < 4; ++i)
    words[i] = uint64_t(low) | (uint64_t(groups[i]) << 32);
  m[1] = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(words));
  for (int i = 2; i < 16; ++i)
    m[i] = _mm256_setzero_si256();
  for (const auto &s : SIGMA) {
    mix4(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
    mix4(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
    mix4(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
    mix4(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
    mix4(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
    mix4(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
    mix4(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
    mix4(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
  }
  for (int i = 0; i < 8; ++i) {
    __m256i word = _mm256_xor_si256(_mm256_set1_epi64x(h[i]),
                                    _mm256_xor_si256(v[i], v[i + 8]));
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(words), word);
    for (int lane = 0; lane < 4; ++lane)
      std::memcpy(out[lane] + i * 8, &words[lane], 8);
  }
}
// A bounded tree walk: 512 21-bit indices, ten pending rows, 64 leaf hashes.
// Every merge checks its 20-bit collision and canonical index order. Global
// distinctness replaces the old repeated pairwise duplicate-index scans.
bool verify_avx2(const uint8_t *header, const uint8_t *solution) {
  std::array<uint32_t, 512> indices;
  uint64_t acc = 0;
  unsigned bits = 0;
  size_t at = 0;
  for (size_t i = 0; i < 1344; ++i) {
    acc = (acc << 8) | solution[i];
    bits += 8;
    if (bits >= 21) {
      bits -= 21;
      indices[at++] = uint32_t((acc >> bits) & 0x1fffff);
    }
  }
  auto sorted = indices;
  std::sort(sorted.begin(), sorted.end());
  if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
    return false;
  uint64_t h[8];
  midstate(header, h);
  uint8_t pending[10][30] = {}, row[30], digests[64][64];
  for (size_t batch = 0; batch < 512; batch += 64) {
    for (size_t j = 0; j < 64; j += 4) {
      uint32_t groups[4];
      for (int lane = 0; lane < 4; ++lane)
        groups[lane] = indices[batch + j + lane] / 2;
      hashes4(h, header + 128, groups, digests + j);
    }
    for (size_t j = 0; j < 64; ++j) {
      size_t leaf = batch + j;
      const uint8_t *digest = digests[j] + (indices[leaf] % 2) * 25;
      uint64_t value = 0;
      unsigned held = 0;
      size_t pos = 0;
      for (size_t k = 0; k < 25; ++k) {
        value = (value << 8) | digest[k];
        held += 8;
        if (held >= 20) {
          held -= 20;
          uint32_t x = uint32_t((value >> held) & 0xfffff);
          row[pos++] = uint8_t(x >> 16);
          row[pos++] = uint8_t(x >> 8);
          row[pos++] = uint8_t(x);
        }
      }
      size_t height = 0;
      while ((leaf >> height) & 1) {
        const size_t len = 30 - height * 3;
        if (std::memcmp(pending[height], row, 3) != 0)
          return false;
        const size_t right = leaf + 1 - (size_t(1) << height),
                     left = right - (size_t(1) << height);
        if (indices[right] < indices[left])
          return false;
        for (size_t k = 0; k < len - 3; ++k)
          row[k] = pending[height][k + 3] ^ row[k + 3];
        ++height;
      }
      std::memcpy(pending[height], row, 30 - height * 3);
    }
  }
  return pending[9][0] == 0 && pending[9][1] == 0 && pending[9][2] == 0;
}
} // namespace
#endif
bool Equihash2009HasAvx2() {
#if defined(EQUIHASH_X86_AVX2)
  static const bool available = __builtin_cpu_supports("avx2");
  return available;
#else
  return false;
#endif
}
bool VerifyEquihash2009(const uint8_t *header, size_t header_len,
                        const uint8_t *solution, size_t solution_len) {
  if (!header || !solution || header_len != 140 || solution_len != 1344)
    return false;
#if defined(EQUIHASH_X86_AVX2)
  if (Equihash2009HasAvx2())
    return verify_avx2(header, solution);
#endif
  eh_HashState state;
  EhInitialiseState(200, 9, state);
  crypto_generichash_blake2b_update(&state, header, header_len);
  return Eh200_9.IsValidSolution(
      state, std::vector<unsigned char>(solution, solution + solution_len));
}
