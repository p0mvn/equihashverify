// Native Zcash (200,9) verification with runtime AVX2 detection.
#ifndef EQUIHASH_AVX2_H
#define EQUIHASH_AVX2_H
#include <stddef.h>
#include <stdint.h>
// Inputs are borrowed readable buffers. Null pointers and wrong lengths fail.
// A true result checks only Equihash, not share target, freshness or
// duplicates.
bool VerifyEquihash2009(const uint8_t *header, size_t header_len,
                        const uint8_t *solution, size_t solution_len);
bool Equihash2009HasAvx2();
#endif
