// Native-only proof and concurrency tests against the original verifier.
#include "crypto/equihash.h"
#include "crypto/equihash_avx2.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
struct Proof {
  std::string name;
  bool valid;
  std::vector<uint8_t> header, solution;
};
std::vector<uint8_t> decode(const std::string &hex) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i < hex.size(); i += 2)
    out.push_back(
        static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  return out;
}
bool original(const Proof &p) {
  eh_HashState state;
  EhInitialiseState(200, 9, state);
  crypto_generichash_blake2b_update(&state, p.header.data(), 140);
  return Eh200_9.IsValidSolution(state, p.solution);
}
bool candidate(const Proof &p) {
  return VerifyEquihash2009(p.header.data(), p.header.size(), p.solution.data(),
                            p.solution.size());
}
int main(int argc, char **argv) {
  if (argc != 2 || sodium_init() < 0)
    return 2;
  if (!freopen("/dev/null", "w", stdout))
    return 2;
  std::ifstream input(argv[1]);
  if (!input)
    return 2;
  std::vector<Proof> cases;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream s(line);
    std::string name, verdict, header, solution;
    if (!(s >> name >> verdict >> header >> solution))
      return 2;
    cases.push_back(
        {name, verdict == "valid", decode(header), decode(solution)});
  }
  if (cases.empty())
    return 2;
  uint8_t byte = 0;
  for (size_t len : std::vector<size_t>{0, 108, 139, 141, size_t(-1)})
    if (VerifyEquihash2009(&byte, len, &byte, 1344))
      return 1;
  for (size_t len : std::vector<size_t>{0, 1343, 1345, size_t(-1)})
    if (VerifyEquihash2009(&byte, 140, &byte, len))
      return 1;
  if (VerifyEquihash2009(nullptr, 140, &byte, 1344) ||
      VerifyEquihash2009(&byte, 140, nullptr, 1344))
    return 1;
  std::mt19937_64 rng(0x5141191);
  for (size_t i = 0; i < 512; ++i) {
    auto p = cases[0];
    p.solution[rng() % p.solution.size()] ^= uint8_t(1u << (rng() % 8));
    p.valid = original(p);
    p.name = "mutated-proof";
    cases.push_back(p);
  }
  size_t valid = 0;
  for (const auto &p : cases) {
    if (original(p) != p.valid || candidate(p) != p.valid) {
      std::cerr << "disagreement: " << p.name << "\n";
      return 1;
    }
    valid += p.valid;
  }
  std::atomic<size_t> failures(0);
  std::vector<std::thread> workers;
  for (size_t t = 0; t < 8; ++t)
    workers.emplace_back([&] {
      for (const auto &p : cases)
        if (candidate(p) != p.valid)
          ++failures;
    });
  for (auto &t : workers)
    t.join();
  std::cerr << "checked=" << cases.size() << " valid=" << valid
            << " AVX2=" << Equihash2009HasAvx2()
            << " concurrent_failures=" << failures << "\n";
  return failures ? 1 : 0;
}
