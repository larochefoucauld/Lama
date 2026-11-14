// Lama bytecode interpreter.

#pragma once

#include "bytecode.hpp"
#include "runtime_api.hpp"
#include "stack.hpp"

namespace lama {

// VM spins interpretation loop over a raw bytecode.
// Malformed bytecode errors are detected lazily during execution.
class VM {
public:
  struct Options {
    bool trace = false; // print every executed instruction
  };

  VM(const Bytecode& bc, Options options);

  void run();

private:
  template <bool Trace>
  void run_loop();

  const Bytecode& bc_;
  Options options_;
  Stack stack_;
  aint* globals_;
};

} // namespace lama
