# Equihash verification for C++ and Node

The `(200,9)` verifier uses one bounded tree walk on every CPU. A four-lane
BLAKE2b kernel uses AVX2 when runtime CPU detection permits it; the scalar twin
uses the same inputs and walk elsewhere. Legacy verification remains available
for the other existing parameter sets. No Rust toolchain is needed.

## C++ integration

On Ubuntu, install `build-essential cmake libsodium-dev libboost-dev`.
The native library needs C++11 and CMake 3.18 or later.
Build with:

```sh
cmake -S . -B native-build -DCMAKE_BUILD_TYPE=Release
cmake --build native-build -j8
ctest --test-dir native-build --output-on-failure
```

Include `crypto/equihash_avx2.h` and call
`VerifyEquihash2009(header, 140, solution, 1344)`. Link
`native-build/libequihashverify.a`, libsodium and pthread. Boost is a header
requirement of the retained legacy verifier; Boost.System is unnecessary.
`-DBUILD_SHARED_LIBS=ON` also builds and tests the shared library.

Buffers are borrowed and must remain readable throughout the call. The
140-byte header includes the nonce and excludes the solution. The 1,344-byte
solution excludes its CompactSize prefix. Null pointers and wrong lengths
return false. Calls are concurrent and the `(200,9)` path neither allocates nor
logs. A valid Equihash proof does not establish share target, freshness,
uniqueness, or credit.

Do not compile the entire library with `-mavx2` or `-march=native` for a mixed
fleet. Only the dispatched hash functions carry an AVX2 target attribute.
Runtime kernel selection is cached, and calls use identical ordering,
distinctness, collision and zero-root checks. Global distinctness makes
comparison of sibling first indices equivalent to lexicographic subtree order.
Ordering and distinctness are checked before any BLAKE2b work.

The new implementation and BLAKE2b specialization follow RFC 7693 and Common
revision `7759b1f5f8b2597599e39610558eaafca752b4b6`,
`crates/equihash/src/leaf_hash.rs` and `verify.rs`, under the MIT license. It
preserves ZcashPoW personalization, a 50-byte digest, little-endian group
indices and 21-bit packed solution indices. All proof inputs are public;
this verifier is not a general-purpose or keyed hash API.

## Tests and required AVX2 qualification

The native tests call the scalar and available AVX2 kernels explicitly, then
check runtime dispatch. Each kernel's leaf hashes are checked directly against
libsodium, including boundary groups 0 and 1,048,575 and random headers. Proof
and concurrent tests compare both kernels with the original verifier.
No test includes an implementation `.cpp` file.

On the x86-64 qualification host, require AVX2 instead of allowing an unavailable
kernel to go untested:

```sh
cmake -S . -B native-build -DCMAKE_BUILD_TYPE=Release -DEQUIHASH_REQUIRE_AVX2=ON
cmake --build native-build -j8
ctest --test-dir native-build --output-on-failure
```

With that option, missing AVX2 is a failure in both hash and proof tests. With
the default OFF, scalar tests always run and AVX2 tests run when available.
The Linux CI job requires AVX2; the portable job tests the same walk and scalar
kernel. Release builds exercise the new verifier and tests with NDEBUG.
Only the retained legacy translation unit keeps assertions, because its unused
sodium signature self-test contains assertion calls with side effects.

`tests/seeds.tsv` holds nine valid independent proofs plus a collision-valid
nonzero-root negative. `python3 tests/reference.py` regenerates `tests/proofs.tsv`
using an independent Python hashlib reference. `--check` verifies regeneration
without changing it. The generated corpus includes subtree swaps and
subtree/index duplication at several heights and offsets, altered headers
including bytes 128 and 139, and proof-byte mutations. Height 8 has only one
512-leaf block, so its block start is necessarily zero. The nonzero-root seed
must pass all nine collision rounds and still be rejected.

For Rosetta, AVX2 advertisement may require `ROSETTA_ADVERTISE_AVX=1`. Let normal
runtime detection choose the kernel; emulated AVX2 runs are not hardware
performance qualification.

## Node integration

Use Node 20 or later and the same system dependencies, then run:

```sh
npm ci
npm test
```

```js
const ev = require('bindings')('equihashverify.node');
ev.verify(headerBuffer, solutionBuffer);         // defaults to (200,9)
ev.verify(headerBuffer, solutionBuffer, 200, 9); // explicit parameters
```

Both forms borrow the buffers for `(200,9)` and return a boolean. Wrong buffer
lengths return false. Non-buffer arguments, wrong argument counts and noninteger
parameters throw TypeError; unsupported parameter pairs throw RangeError.
The existing supported parameter pairs remain available. The asserting Mocha
suite reads the same proof corpus and tests both forms plus malformed API input.
The NAN binding uses context-aware conversion helpers and C++20 for current
Node headers. The public Node suite has been exercised on Node 22 and Node 24.

The workflow file is supplied for future pushes. No hosted CI result is implied
by local tests or by the presence of that file.

## Reproducing the native benchmark

Enable `-DEQUIHASH_BUILD_BENCHMARK=ON` in the release configuration above.
`native-build/pool_bench avx2 tests/proofs.tsv valid 1 2000` emits CSV on stderr:
backend, workload, threads, total shares, wall ns, process CPU ns, CPU ns/share,
p50 ns, p95 ns, p99 ns and failure count. Backends are `legacy`, `scalar` and
`avx2`; workloads are `valid`, `early` (duplicate index), and `late` (nonzero
root). Use threads 1 and 8; interleave at least three runs on one quiet host.
An unavailable explicitly selected AVX2 backend fails. Legacy invalid-share
logging is preserved but redirected to `/dev/null` by the benchmark.

The portable oracle test can run without target-platform libsodium or Boost:

```sh
aarch64-linux-gnu-g++ -O3 -DNDEBUG -std=c++11 -static -pthread -I. \
  crypto/equihash_200_9.cpp crypto/equihash_avx2.cpp tests/portable.cpp \
  -o portable-aarch64
qemu-aarch64 ./portable-aarch64 tests/proofs.tsv tests/hashes.tsv
```

This checks the production scalar/walk code against independent proof and hash
vectors on an emulated architecture. Hardware qualification is a separate step.
