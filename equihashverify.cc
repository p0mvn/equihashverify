#include "crypto/equihash.h"
#include "crypto/equihash_avx2.h"
#include <exception>
#include <nan.h>
#include <node_buffer.h>
#include <vector>

int verifyEH(const char *header, const std::vector<unsigned char> &solution,
             unsigned int n = 200, unsigned int k = 9) {
  if (n == 200 && k == 9)
    return VerifyEquihash2009(reinterpret_cast<const uint8_t *>(header), 140,
                              solution.data(), solution.size());
  eh_HashState state;
  EhInitialiseState(n, k, state);
  crypto_generichash_blake2b_update(
      &state, reinterpret_cast<const uint8_t *>(header), 140);
  if (n == 96 && k == 3)
    return Eh96_3.IsValidSolution(state, solution);
  if (n == 144 && k == 5)
    return Eh144_5.IsValidSolution(state, solution);
  if (n == 192 && k == 7)
    return Eh192_7.IsValidSolution(state, solution);
  if (n == 96 && k == 5)
    return Eh96_5.IsValidSolution(state, solution);
  if (n == 48 && k == 5)
    return Eh48_5.IsValidSolution(state, solution);
  throw std::invalid_argument("Unsupported Equihash parameters");
}

NAN_METHOD(Verify) {
  if (info.Length() != 2 && info.Length() != 4)
    return Nan::ThrowTypeError("Expected header, solution and optional n, k");
  for (int i = 0; i < 2; ++i) {
    if (!node::Buffer::HasInstance(info[i]))
      return Nan::ThrowTypeError("Header and solution must be buffers");
    const auto is_buffer = info[i]->InstanceOf(
        Nan::GetCurrentContext(), info.Data().As<v8::Object>());
    if (is_buffer.IsNothing())
      return; // Preserve an exception from a custom Symbol.hasInstance.
    if (!is_buffer.FromJust())
      return Nan::ThrowTypeError("Header and solution must be buffers");
  }
  unsigned n = 200, k = 9;
  if (info.Length() == 4) {
    if (!info[2]->IsUint32() || !info[3]->IsUint32())
      return Nan::ThrowTypeError(
          "Equihash parameters must be unsigned integers");
    n = Nan::To<uint32_t>(info[2]).FromJust();
    k = Nan::To<uint32_t>(info[3]).FromJust();
  }
  if (!((n == 200 && k == 9) || (n == 96 && k == 3) || (n == 144 && k == 5) ||
        (n == 192 && k == 7) || (n == 96 && k == 5) || (n == 48 && k == 5)))
    return Nan::ThrowRangeError("Unsupported Equihash parameters");
  const auto *header =
      reinterpret_cast<const uint8_t *>(node::Buffer::Data(info[0]));
  const auto *solution =
      reinterpret_cast<const uint8_t *>(node::Buffer::Data(info[1]));
  const size_t header_len = node::Buffer::Length(info[0]);
  const size_t solution_len = node::Buffer::Length(info[1]);
  if (n == 200 && k == 9) {
    info.GetReturnValue().Set(
        VerifyEquihash2009(header, header_len, solution, solution_len));
    return;
  }
  const size_t expected = (size_t(1) << k) * (n / (k + 1) + 1) / 8;
  if (header_len != 140 || solution_len != expected) {
    info.GetReturnValue().Set(false);
    return;
  }
  try {
    const std::vector<unsigned char> bytes(solution, solution + solution_len);
    info.GetReturnValue().Set(
        bool(verifyEH(reinterpret_cast<const char *>(header), bytes, n, k)));
  } catch (const std::exception &error) {
    Nan::ThrowError(error.what());
  } catch (...) {
    Nan::ThrowError("Equihash verification failed");
  }
}
NAN_MODULE_INIT(Init) {
  // Node creates this buffer without consulting global.Buffer. Keep its
  // constructor with the callback so replacing that global cannot change
  // validation, even when the addon is loaded after the replacement.
  v8::Local<v8::Object> buffer;
  v8::Local<v8::Value> constructor;
  if (!Nan::NewBuffer(0).ToLocal(&buffer) ||
      !Nan::Get(buffer, Nan::New("constructor").ToLocalChecked())
           .ToLocal(&constructor))
    return;
  if (!constructor->IsFunction())
    return Nan::ThrowTypeError("Native Buffer constructor is unavailable");
  Nan::SetMethod(target, "verify", Verify, constructor);
}
NODE_MODULE(equihashverify, Init)
