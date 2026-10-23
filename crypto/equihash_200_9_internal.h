// Internal implementation boundary shared by the walk, hash kernels and tests.
#ifndef EQUIHASH_200_9_INTERNAL_H
#define EQUIHASH_200_9_INTERNAL_H
#include <array>
#include <cstddef>
#include <cstdint>
namespace equihash2009 {
extern const uint64_t IV[8];
extern const unsigned char SIGMA[12][16];
inline uint64_t Load64(const uint8_t *p) {
  uint64_t value = 0;
  for (unsigned i = 0; i < 8; ++i)
    value |= uint64_t(p[i]) << (8 * i);
  return value;
}
using HashFour = void (*)(const uint64_t *, const uint8_t *, const uint32_t *,
                          uint8_t (*)[64]);
void InitialiseMidstate(const uint8_t *header, uint64_t *state);
void ScalarHashFour(const uint64_t *state, const uint8_t *tail,
                    const uint32_t *groups, uint8_t out[4][64]);
// Returns nullptr if this build or CPU cannot execute the AVX2 kernel.
HashFour Avx2HashFour();
HashFour SelectedHashFour();
// solution points to exactly 1344 readable bytes. Output contains 512 indices.
bool DecodeAndCheckIndices(const uint8_t *solution,
                           std::array<uint32_t, 512> &indices);
// Explicit kernel selection for native tests and benchmarks; never falls back.
bool VerifyWithHashFour(HashFour hash_four, const uint8_t *header,
                        size_t header_len, const uint8_t *solution,
                        size_t solution_len);
} // namespace equihash2009
#endif
