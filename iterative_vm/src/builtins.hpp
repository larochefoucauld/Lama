// Lama runtime builtin calling support

#pragma once

#include "runtime_api.hpp"

#include <string_view>
#include <variant>

namespace lama {

// Supported builtin signatures
using Fn0 = aint (*)();
using Fn1 = aint (*)(aint);
using Fn2 = aint (*)(aint, aint);
using Fn3 = aint (*)(aint, aint, aint);
using FnArgs = aint (*)(aint* args);

// TODO: support varargs

struct Builtin {
  std::string_view name;
  std::variant<Fn0, Fn1, Fn2, Fn3, FnArgs> fn;
  unsigned arity;

  Builtin(std::string_view name, Fn0 f)
      : name(name)
      , fn(f)
      , arity(0) {}

  Builtin(std::string_view name, Fn1 f)
      : name(name)
      , fn(f)
      , arity(1) {}

  Builtin(std::string_view name, Fn2 f)
      : name(name)
      , fn(f)
      , arity(2) {}

  Builtin(std::string_view name, Fn3 f)
      : name(name)
      , fn(f)
      , arity(3) {}

  Builtin(std::string_view name, FnArgs f, unsigned arity)
      : name(name)
      , fn(f)
      , arity(arity) {}
};

const Builtin* find_builtin(std::string_view name);

// Call builtin passing the arguments from the stack.
// Stack grows down, so `sp` points to the last argument.
aint call_builtin(const Builtin& b, aint* sp, unsigned n_args);

} // namespace lama
