#include "builtins.hpp"

#include "bytecode.hpp"

#include <algorithm>
#include <string>

namespace lama {

namespace {

const Builtin Builtins[] = {
    {"read", Lread},
    {"write", Lwrite},
    {"length", Llength},
    {"string", Lstring, 1},
    {"stringcat", Lstringcat, 1},
    {"clone", Lclone, 1},
    {"substring", Lsubstring, 3},
    {"i__Infix_4343", Li__Infix_4343, 2},
    {"s__Infix_58", Ls__Infix_58, 2},
    {"fst", Lfst},
    {"snd", Lsnd},
    {"hd", Lhd},
    {"tl", Ltl},
    {"kindOf", LkindOf},
    {"compare", Lcompare},
    {"flatCompare", LflatCompare},
    {"compareTags", LcompareTags},
    {"hash", Lhash},
    {"uppercase", Luppercase},
    {"lowercase", Llowercase},
    {"makeArray", LmakeArray},
    {"makeString", LmakeString},
    {"random", Lrandom},
    {"time", Ltime},
    {"readLine", LreadLine},
    {"matchSubString", LmatchSubString},
    {"regexp", Lregexp},
    {"regexpMatch", LregexpMatch},
    {"stringInt", LstringInt},
    {"getEnv", LgetEnv},
    {"system", Lsystem}
};

// i-th argument is at sp[n - 1 - i]
struct Caller {
  aint* sp;
  unsigned n;

  aint arg(unsigned i) const {
    return sp[n - 1 - i];
  }

  aint operator()(Fn0 f) const {
    return f();
  }

  aint operator()(Fn1 f) const {
    return f(arg(0));
  }

  aint operator()(Fn2 f) const {
    return f(arg(0), arg(1));
  }

  aint operator()(Fn3 f) const {
    return f(arg(0), arg(1), arg(2));
  }

  aint operator()(FnArgs f) const {
    std::reverse(sp, sp + n);
    return f(sp);
  }
};

} // namespace

const Builtin* find_builtin(std::string_view name) {
  for (const Builtin& b : Builtins) {
    if (b.name == name) {
      return &b;
    }
  }
  return nullptr;
}

aint call_builtin(const Builtin& b, aint* sp, unsigned n_args) {
  if (n_args != b.arity) {
    throw BytecodeError(
        "builtin '" + std::string(b.name) + "' expects " + std::to_string(b.arity) + " argument(s), got " +
        std::to_string(n_args)
    );
  }
  return std::visit(Caller{sp, n_args}, b.fn);
}

} // namespace lama
