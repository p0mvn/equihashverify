// Check the new hashing kernel against the independent libsodium primitive.
#include "../crypto/equihash_avx2.cpp"
#include <iostream>
#include <random>
int main() {
#if defined(EQUIHASH_X86_AVX2)
  if (!Equihash2009HasAvx2())
    return 77;
  std::mt19937_64 rng(0x5141191);
  for (size_t trial = 0; trial < 256; ++trial) {
    uint8_t header[140];
    for (auto &b : header)
      b = static_cast<uint8_t>(rng());
    uint64_t h[8];
    midstate(header, h);
    uint32_t groups[4] = {0, 1, 1048575,
                          static_cast<uint32_t>(rng() & 1048575)};
    uint8_t actual[4][64];
    hashes4(h, header + 128, groups, actual);
    uint8_t personal[16] = {};
    std::memcpy(personal, "ZcashPoW", 8);
    personal[8] = 200;
    personal[12] = 9;
    for (size_t lane = 0; lane < 4; ++lane) {
      eh_HashState state;
      uint8_t expected[50];
      if (crypto_generichash_blake2b_init_salt_personal(&state, nullptr, 0, 50,
                                                        nullptr, personal) != 0)
        return 1;
      crypto_generichash_blake2b_update(&state, header, 140);
      // This test runs on x86-64, whose native uint32_t encoding is little
      // endian.
      crypto_generichash_blake2b_update(
          &state, reinterpret_cast<uint8_t *>(&groups[lane]), 4);
      crypto_generichash_blake2b_final(&state, expected, 50);
      if (std::memcmp(expected, actual[lane], 50) != 0) {
        std::cerr << "hash mismatch " << trial << " " << lane << "\n";
        return 1;
      }
    }
  }
  std::cerr << "1024 AVX2 leaf hashes match libsodium, including boundary "
               "group indices\n";
  return 0;
#else
  return 77;
#endif
}
