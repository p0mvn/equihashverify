# Equihash - Zcash Implementation
nodejs native binding to check for valid Equihash solutions

# Dependencies
````
sudo apt-get install build-essential libsodium-dev libboost-system-dev
````

# Usage
````javascript
var ev = require('bindings')('equihashverify.node');

var header = new Buffer(..., 'hex');
var solution = new Buffer(..., 'hex'); //do not include byte size preamble "fd4005"

ev.verify(header, solution, n, k);
//returns boolean
````

# Backward compatibility
````javascript
ev.verify(header, solution);
````

# Test Suite:
````
sudo npm install -g mocha
npm install
mocha
````


# Native C++ AVX2 verification

The (200,9) path now verifies proofs using four parallel BLAKE2b leaf hashes
and a bounded single-pass tree walk. Runtime CPU detection enables AVX2 only
on supported x86-64 CPUs; other CPUs use the existing verifier. No Rust
toolchain or library is required. The `verifyEH` signature and Node buffer
interface are preserved, including the existing non-(200,9) parameter paths.

For a native pool, include `crypto/equihash_avx2.h` and call
`VerifyEquihash2009(header, 140, solution, 1344)`. Inputs must be readable for
the duration of the call; buffers are borrowed and calls may run concurrently.
The header includes its nonce and excludes the solution. The solution excludes
the CompactSize prefix. Null pointers and wrong lengths return false. A valid
proof does not establish share difficulty, freshness, uniqueness, or credit.

Build the native library with `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
and `cmake --build build -j8`. Link `build/libequihashverify.a`, libsodium,
Boost.System and pthread. Do not enable AVX2 globally: only the dispatched
BLAKE2b function is compiled for AVX2. The existing legacy source requires
assertions, which CMake retains even in release builds.

The new BLAKE2b implementation and tree walk are adapted from
`zakura-core/common` revision 7759b1f5f8b2597599e39610558eaafca752b4b6,
`crates/equihash/src/leaf_hash.rs` and `verify.rs`, under the MIT license.
The algorithm follows RFC 7693 and preserves the `ZcashPoW` personalization,
50-byte hash output, 21-bit solution indices and all Equihash (200,9) checks.
