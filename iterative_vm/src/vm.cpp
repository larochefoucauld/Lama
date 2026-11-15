#include "vm.hpp"

#include "builtins.hpp"
#include "opcodes.hpp"
#include "runtime_api.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace lama {

namespace {

constexpr std::size_t StackBytes = std::size_t{64} << 20;

// Call frame layout:
//   bp[0] = n_args
//   bp[-1] = saved bp
//   bp[-2] = saved ip (offset)
//   bp[-3] = caller's sp after the call
//   bp[-4 - i] = local i
//   bp[n_args - i] = argument i
//   bp[n_args + 1] = closure (in closure frames)
constexpr int SavedBP = -1;
constexpr int SavedIP = -2;
constexpr int SavedSP = -3;
constexpr int FrameWords = 4;
constexpr int FirstLocal = -4;

constexpr aint HostReturn = -1; // saved ip of the entry frame

// Runtime initialization and teardown
struct RuntimeSession {
  RuntimeSession() {
    __init();
  }

  ~RuntimeSession() {
    __shutdown();
  }

  RuntimeSession(const RuntimeSession&) = delete;
  RuntimeSession& operator=(const RuntimeSession&) = delete;
};

// This must be called before every call of runtime API
// that may allocate
inline void sync_gc(aint* sp) {
  __gc_stack_top = static_cast<std::size_t>(as_word(sp - 1));
}

[[noreturn]] void fail_at(const std::string& what, std::ptrdiff_t offset) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), " at offset 0x%.8tx", offset);
  throw BytecodeError(what + buf);
}

std::string hex8(std::int64_t v) {
  assert(v >= 0);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "0x%.8llx", static_cast<unsigned long long>(v));
  return buf;
}

} // namespace

VM::VM(const Bytecode& bc, Options options)
    : bc_(bc)
    , options_(options)
    , stack_(StackBytes) {
  for (std::string_view import : bc.imports()) {
    if (import != "Std") {
      // TODO
      throw BytecodeError("import of unit '" + std::string(import) + "' is not supported yet");
    }
  }
  if (bc.globals_count() > StackBytes / sizeof(aint) / 2) {
    throw BytecodeError("too many global variables: " + std::to_string(bc.globals_count()));
  }
  globals_ = stack_.top() - bc.globals_count();
  for (std::size_t i = 0; i < bc.globals_count(); i++) {
    globals_[i] = box(0);
  }
}

void VM::run() {
  RuntimeSession session;
  __gc_stack_bottom = static_cast<std::size_t>(as_word(stack_.top()));
  if (options_.trace) {
    run_loop<true>();
  } else {
    run_loop<false>();
  }
}

template <bool Trace>
void VM::run_loop() {
  const std::span<const std::uint8_t> code = bc_.code();
  const std::uint8_t* const base = code.data();
  const std::uint8_t* const end = base + code.size();
  aint* const globals = globals_;
  const std::size_t n_globals = bc_.globals_count();
  aint* const stack_limit = stack_.limit();
  std::unordered_map<std::int32_t, const Builtin*> builtin_cache;

  const std::uint8_t* ip = base;
  const std::uint8_t* insn = base; // start of the current instruction
  aint* sp = globals;
  aint* bp = nullptr;

#define FAIL(msg) fail_at((msg), insn - base)

  auto get_operand_at = [&]<typename T>(const std::uint8_t* ip) -> T {
    if (ip + sizeof(T) > end) {
      FAIL("malformed bytecode: truncated operand");
    }
    T v;
    std::memcpy(&v, ip, sizeof(v));
    return v;
  };
  auto opd_u8_at = [&](const std::uint8_t* ip) -> std::uint8_t {
    return get_operand_at.template operator()<std::uint8_t>(ip);
  };
  auto opd_i32_at = [&](const std::uint8_t* ip) -> std::int32_t {
    return get_operand_at.template operator()<std::int32_t>(ip);
  };
  auto opd_u8 = [&]() -> std::uint8_t {
    auto v = opd_u8_at(ip);
    ip += sizeof(v);
    return v;
  };
  auto opd_i32 = [&]() -> std::int32_t {
    auto v = opd_i32_at(ip);
    ip += sizeof(v);
    return v;
  };

  auto push = [&](aint v) {
    if (sp == stack_limit) {
      FAIL("stack overflow");
    }
    *--sp = v;
  };
  auto pop = [&]() -> aint {
    if (sp >= bp) {
      FAIL("malformed bytecode: operand stack underflow");
    }
    return *sp++;
  };
  auto peek = [&]() -> aint {
    if (sp >= bp) {
      FAIL("malformed bytecode: operand stack underflow");
    }
    return *sp;
  };
  auto reverse = [&](std::size_t n) -> void {
    if (sp + n >= bp) {
      FAIL("malformed bytecode: operand stack underflow");
    }
    std::reverse(sp, sp + n);
  };
  auto jump_to = [&](std::int32_t target) {
    if (static_cast<std::size_t>(target) >= code.size()) {
      FAIL("malformed bytecode: jump target " + hex8(target) + " out of range");
    }
    ip = base + target;
  };

  auto string_at = [&](std::int32_t offset) -> std::string_view {
    try {
      return bc_.string_at(offset);
    } catch (const BytecodeError& e) {
      FAIL(std::string("malformed bytecode: ") + e.what());
    }
  };
  auto tag_hash = [&](std::int32_t offset) -> aint {
    return LtagHash(string_at(offset).data());
  };
  auto resolve_builtin = [&](std::int32_t target) -> const Builtin& {
    auto it = builtin_cache.find(target);
    if (it != builtin_cache.end()) {
      return *it->second;
    }
    std::int32_t offset = static_cast<std::int32_t>(-static_cast<std::int64_t>(target) - 1);
    std::string_view name = string_at(offset);
    const Builtin* b = find_builtin(name);
    if (!b) {
      FAIL("unsupported builtin function '" + std::string(name) + "'");
    }
    builtin_cache.emplace(target, b);
    return *b;
  };

  auto get_global = [&](std::int32_t idx) -> aint& {
    if (idx < 0) {
      FAIL("external global variables are not supported yet");
    }
    if (static_cast<std::size_t>(idx) >= n_globals) {
      FAIL("malformed bytecode: global index " + std::to_string(idx) + " out of range");
    }
    return globals[idx];
  };
  auto get_local = [&](std::int32_t idx) -> aint& {
    // Locals live between the frame header and the stack pointer
    if (idx < 0 || bp + FirstLocal - idx < sp) {
      FAIL("malformed bytecode: local index " + std::to_string(idx) + " out of range");
    }
    return *(bp + FirstLocal - idx);
  };
  auto get_arg = [&](std::int32_t idx) -> aint& {
    aint n_args = bp[0];
    if (idx < 0 || idx >= n_args) {
      FAIL("malformed bytecode: argument index " + std::to_string(idx) + " out of range");
    }
    return *(bp + (n_args - idx));
  };
  auto get_captured = [&](std::int32_t idx) -> aint& {
    aint n_args = bp[0];
    // TODO: check that we are in closure
    aint* closure = as_ptr<aint>(bp[n_args + 1]);
    // TODO: check that idx is in bounds
    return closure[idx + 1];
  };

  // Push call frame for the function entry at `target` and jump there.
  // `caller_sp` is used to restore the caller's frame after function arguments
  // are popped.
  auto push_frame = [&](std::int32_t target, std::int32_t n_args, aint* caller_sp) {
    if (sp - FrameWords < stack_limit) {
      FAIL("stack overflow");
    }
    push(n_args);
    aint* new_bp = sp;
    push(as_word(bp));
    push(ip - base); // return address: the instruction after the call
    push(as_word(caller_sp));
    bp = new_bp;
    ip = base + target;
  };

  // Setup the entry frame
  if (code.empty() || static_cast<Op>(base[0]) != Op::Begin) {
    FAIL("malformed bytecode: code does not start with BEGIN");
  }
  push(0); // n_args
  bp = sp;
  push(0); // saved bp
  push(HostReturn);
  push(as_word(globals));

  for (;;) {
    // Run interpreter step
    if (ip >= end) {
      FAIL("malformed bytecode: execution ran past the end of the code");
    }
    insn = ip;
    const std::uint8_t opcode = *ip++;

    if constexpr (Trace) {
      const char* name = op_name(opcode);
      std::fprintf(stderr, "lamai: trace 0x%.8tx %-13s depth=%td", insn - base, name ? name : "?", globals - sp);
      if (sp < globals) {
        if (is_unboxed(*sp)) {
          std::fprintf(stderr, " top=%lld", static_cast<long long>(unbox(*sp)));
        } else {
          std::fprintf(stderr, " top=ptr 0x%llx", static_cast<unsigned long long>(*sp));
        }
      }
      std::fputc('\n', stderr);
    }

    switch (static_cast<Op>(opcode)) {
#define BINOP_CASE(op, fn)                                                                                             \
  case Op::op: {                                                                                                       \
    aint y = pop();                                                                                                    \
    aint x = pop();                                                                                                    \
    push(fn(x, y));                                                                                                    \
    break;                                                                                                             \
  }
      BINOP_CASE(Add, Ls__Infix_43)
      BINOP_CASE(Sub, Ls__Infix_45)
      BINOP_CASE(Mul, Ls__Infix_42)
      BINOP_CASE(Div, Ls__Infix_47)
      BINOP_CASE(Mod, Ls__Infix_37)
      BINOP_CASE(Lt, Ls__Infix_60)
      BINOP_CASE(Le, Ls__Infix_6061)
      BINOP_CASE(Gt, Ls__Infix_62)
      BINOP_CASE(Ge, Ls__Infix_6261)
      BINOP_CASE(Eq, Ls__Infix_6161)
      BINOP_CASE(Ne, Ls__Infix_3361)
      BINOP_CASE(And, Ls__Infix_3838)
      BINOP_CASE(Or, Ls__Infix_3333)
      BINOP_CASE(PattStrCmp, Bstring_patt)
#undef BINOP_CASE

#define UNOP_CASE(op, fn)                                                                                              \
  case Op::op: {                                                                                                       \
    push(fn(pop()));                                                                                                   \
    break;                                                                                                             \
  }
      UNOP_CASE(PattString, Bstring_tag_patt)
      UNOP_CASE(PattArray, Barray_tag_patt)
      UNOP_CASE(PattSexp, Bsexp_tag_patt)
      UNOP_CASE(PattBoxed, Bboxed_patt)
      UNOP_CASE(PattUnboxed, Bunboxed_patt)
      UNOP_CASE(PattClosure, Bclosure_tag_patt)
#undef UNOP_CASE

    case Op::Const:
      push(box(opd_i32()));
      break;
    case Op::Drop:
      pop();
      break;
    case Op::Dup:
      push(peek());
      break;
    case Op::Elem: {
      aint idx = pop();
      aint arr = pop();
      push(Belem(arr, idx));
      break;
    }
    case Op::String: {
      const char* str = string_at(opd_i32()).data();
      sync_gc(sp);
      push(Bstring(&str));
      break;
    }
    case Op::Sta: {
      aint val = pop();
      aint idx = pop();
      aint agg = pop();
      Bsta(agg, idx, val);
      push(val);
      break;
    }
    case Op::Barray: {
      auto n = opd_i32();
      if (n < 0) {
        FAIL("malformed bytecode: negative BARRAY length");
      }
      reverse(n);
      sync_gc(sp);
      aint result = Barray(sp, box(n));
      sp += n;
      push(result);
      break;
    }
    case Op::Sexp: {
      aint hash = tag_hash(opd_i32());
      std::int32_t n_fields = opd_i32();
      if (n_fields < 0) {
        FAIL("malformed bytecode: negative SEXP field count");
      }
      const aint count = static_cast<aint>(n_fields) + 1;
      push(hash);
      reverse(count);
      sync_gc(sp);
      aint result = Bsexp(sp, box(count));
      sp += count;
      push(result);
      break;
    }
    case Op::Tag: {
      aint hash = tag_hash(opd_i32());
      std::int32_t n_fields = opd_i32();
      push(Btag(pop(), hash, box(n_fields)));
      break;
    }
    case Op::Array: {
      std::int32_t n = opd_i32();
      push(Barray_patt(pop(), box(n)));
      break;
    }
    case Op::Fail:
    case Op::FailKeep: {
      // The value stays on the stack in both variants: VM exits here
      std::int32_t line = opd_i32();
      std::int32_t col = opd_i32();
      aint value = peek();
      sync_gc(sp);
      Bmatch_failure(value, bc_.name().c_str(), box(line), box(col));
    }
    case Op::Swap: {
      aint a = pop();
      aint b = pop();
      push(a);
      push(b);
      break;
    }
    case Op::Line:
      opd_i32();
      break;

    case Op::Jmp:
      jump_to(opd_i32());
      break;
    case Op::CjmpZ: {
      std::int32_t target = opd_i32();
      if (unbox(pop()) == 0) {
        jump_to(target);
      }
      break;
    }
    case Op::CjmpNz: {
      std::int32_t target = opd_i32();
      if (unbox(pop()) != 0) {
        jump_to(target);
      }
      break;
    }

    case Op::LdGlo:
    case Op::StGlo: {
      std::int32_t idx = opd_i32();
      if (static_cast<Op>(opcode) == Op::LdGlo) {
        push(get_global(idx));
      } else {
        get_global(idx) = peek();
      }
      break;
    }
    case Op::LdLoc:
      push(get_local(opd_i32()));
      break;
    case Op::StLoc:
      get_local(opd_i32()) = peek();
      break;
    case Op::LdArg:
      push(get_arg(opd_i32()));
      break;
    case Op::StArg:
      get_arg(opd_i32()) = peek();
      break;
    case Op::LdClo:
      push(get_captured(opd_i32()));
      break;
    case Op::StClo:
      get_captured(opd_i32()) = peek();
      break;
    case Op::Begin:
    case Op::BeginClosure: {
      std::int32_t n_args = opd_i32();
      std::int32_t n_locals = opd_i32();
      std::int32_t n_captured = static_cast<Op>(opcode) == Op::BeginClosure ? opd_i32() : 0;
      if (n_args < 0 || n_locals < 0 || n_captured < 0) {
        FAIL("malformed bytecode: negative BEGIN operand");
      }
      if (sp - n_locals < stack_limit) {
        FAIL("stack overflow");
      }
      for (std::int32_t i = 0; i < n_locals; i++) {
        push(box(0));
      }
      break;
    }
    case Op::Call: {
      std::int32_t target = opd_i32();
      std::int32_t n_args = opd_i32();
      if (n_args < 0 || sp + n_args > bp) {
        FAIL(
            "malformed bytecode: CALL with " + std::to_string(n_args) + " argument(s) but the frame holds " +
            std::to_string(bp - sp) + " word(s)"
        );
      }
      if (target < 0) {
        const Builtin& b = resolve_builtin(target);
        sync_gc(sp);
        aint result = call_builtin(b, sp, n_args);
        sp += n_args;
        push(result);
        break;
      }
      if (static_cast<std::size_t>(target) >= code.size() || static_cast<Op>(base[target]) != Op::Begin) {
        FAIL("malformed bytecode: CALL target " + hex8(target) + " is not a function entry");
      }
      auto target_n_args = opd_i32_at(base + target + 1);
      if (n_args != target_n_args) {
        FAIL(
            "malformed bytecode: CALL target " + hex8(target) + " has unexpected number of arguments (expected: " +
            std::to_string(n_args) + ", actual: " + std::to_string(target_n_args) + ")"
        );
      }
      push_frame(target, n_args, sp + n_args);
      break;
    }
    case Op::Callc: {
      std::int32_t n_args = opd_i32();
      // The closure value is above the arguments
      if (n_args < 0 || sp + n_args + 1 > bp) {
        FAIL(
            "malformed bytecode: CALLC with " + std::to_string(n_args) + "+1 arguments but the frame holds " +
            std::to_string(bp - sp) + " word(s)"
        );
      }
      aint closure = sp[n_args];
      if (unbox(Bclosure_tag_patt(closure)) == 0) {
        FAIL("CALLC: the called value is not a closure");
      }
      aint code_ref = as_ptr<aint>(closure)[0]; // boxed offset
      if (!is_unboxed(code_ref) || static_cast<std::size_t>(unbox(code_ref)) >= code.size()) {
        FAIL("malformed closure: invalid code reference");
      }
      auto target = static_cast<std::int32_t>(unbox(code_ref));
      Op target_op = static_cast<Op>(base[target]);
      if (target_op != Op::Begin && target_op != Op::BeginClosure) {
        FAIL("malformed bytecode: CALLC target " + hex8(target) + " is not a function entry");
      }
      auto target_n_args = opd_i32_at(base + target + 1);
      if (n_args != target_n_args) {
        FAIL(
            "malformed bytecode: CALLC target " + hex8(target) + " has unexpected number of arguments (expected: " +
            std::to_string(n_args) + ", actual: " + std::to_string(target_n_args) + ")"
        );
      }
      push_frame(target, n_args, sp + n_args + 1);
      break;
    }
    case Op::Closure: {
      auto target = opd_i32();
      if (target < 0) {
        FAIL("external CLOSURE targets are not supported yet");
      }
      if (static_cast<std::size_t>(target) >= code.size() ||
          (static_cast<Op>(base[target]) != Op::Begin && static_cast<Op>(base[target]) != Op::BeginClosure)) {
        FAIL("malformed bytecode: CLOSURE target " + hex8(target) + " is not a closure entry");
      }
      auto n_captured = opd_i32();
      if (n_captured < 0) {
        FAIL("malformed bytecode: negative number of captures");
      }
      Op target_op = static_cast<Op>(base[target]);
      auto target_n_captured = target_op == Op::Begin ? 0 : opd_i32_at(base + target + 9);
      if (n_captured != target_n_captured) {
        FAIL(
            "malformed bytecode: CLOSURE target " + hex8(target) + " has unexpected number of captures (expected: " +
            std::to_string(n_captured) + ", actual: " + std::to_string(target_n_captured) + ")"
        );
      }
      for (std::size_t i = 0; i < static_cast<std::size_t>(n_captured); ++i) {
        auto kind = opd_u8();
        auto idx = opd_i32();
        switch (static_cast<ClosureCaptureKind>(kind)) {
#define CAPTURE_CASE(kind, fn)                                                                                         \
  case ClosureCaptureKind::kind: {                                                                                     \
    push(fn(idx));                                                                                                     \
    break;                                                                                                             \
  }
          CAPTURE_CASE(Global, get_global)
          CAPTURE_CASE(Local, get_local)
          CAPTURE_CASE(Arg, get_arg)
          CAPTURE_CASE(Captured, get_captured)
#undef CAPTURE_CASE
        default:
          FAIL("malformed bytecode: unexpected capture kind: " + std::to_string(kind));
        }
      }
      reverse(n_captured);
      push(box(target)); // box code offset to be ignored by GC
      sync_gc(sp);
      aint result = Bclosure(sp, box(n_captured));
      sp += n_captured + 1;
      push(result);
      break;
    }
    case Op::End: {
      aint result = pop();
      aint saved_ip = bp[SavedIP];
      sp = as_ptr<aint>(bp[SavedSP]);
      bp = as_ptr<aint>(bp[SavedBP]);
      if (saved_ip == HostReturn) {
        return;
      }
      ip = base + saved_ip;
      push(result);
      break;
    }
    case Op::Eof:
      FAIL("malformed bytecode: EOF reached");

    default: {
      const char* name = op_name(opcode);
      if (!name) {
        FAIL("malformed bytecode: unknown opcode " + std::to_string(opcode));
      }
      FAIL(std::string("opcode ") + name + " is not implemented yet");
    }
    }
  }
#undef FAIL
}

} // namespace lama
