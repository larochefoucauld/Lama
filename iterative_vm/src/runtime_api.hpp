// Lama C runtime compatibility layer

#pragma once

#include <cstddef>
#include <cstdint>

// Redefine certain APIs in a more C++ style.
namespace lama {

using aint = std::int64_t; // the runtime's word type on x86-64
static_assert(sizeof(aint) == sizeof(void*));

// Integers are tagged with the low bit set,
// heap objects are word-aligned pointers
constexpr aint box(aint x) {
  return static_cast<aint>(static_cast<std::uint64_t>(x) << 1) | 1;
}

constexpr aint unbox(aint x) {
  return x >> 1;
}

constexpr bool is_unboxed(aint x) {
  return (x & 1) != 0;
}

inline aint as_word(const void* p) {
  return reinterpret_cast<aint>(p);
}

template <class T>
T* as_ptr(aint w) {
  return reinterpret_cast<T*>(w);
}

} // namespace lama

// We use the changed signatures of original runtime API
// declarations to reduce the amount of cast boilerplate when calling.
// Linking is enabled by ABI equivalence on x86-64 System V targets.
extern "C" {

void __init(void);
void __shutdown(void);
extern std::size_t __gc_stack_top, __gc_stack_bottom;

// Binary operators
lama::aint Ls__Infix_43(lama::aint, lama::aint); // +
lama::aint Ls__Infix_45(lama::aint, lama::aint); // -
lama::aint Ls__Infix_42(lama::aint, lama::aint); // *
lama::aint Ls__Infix_47(lama::aint, lama::aint); // /
lama::aint Ls__Infix_37(lama::aint, lama::aint); // %
lama::aint Ls__Infix_60(lama::aint, lama::aint); // <
lama::aint Ls__Infix_6061(lama::aint, lama::aint); // <=
lama::aint Ls__Infix_62(lama::aint, lama::aint); // >
lama::aint Ls__Infix_6261(lama::aint, lama::aint); // >=
lama::aint Ls__Infix_6161(lama::aint, lama::aint); // ==
lama::aint Ls__Infix_3361(lama::aint, lama::aint); // !=
lama::aint Ls__Infix_3838(lama::aint, lama::aint); // &&
lama::aint Ls__Infix_3333(lama::aint, lama::aint); // !!

// Builtins
lama::aint Lread(void);
lama::aint Lwrite(lama::aint);
lama::aint Llength(lama::aint);
lama::aint Lstring(lama::aint* args);
lama::aint Lstringcat(lama::aint* args);
lama::aint Lclone(lama::aint* args);
lama::aint Lsubstring(lama::aint* args);
lama::aint Li__Infix_4343(lama::aint* args);
lama::aint Ls__Infix_58(lama::aint* args);
lama::aint Lfst(lama::aint);
lama::aint Lsnd(lama::aint);
lama::aint Lhd(lama::aint);
lama::aint Ltl(lama::aint);
lama::aint LkindOf(lama::aint);
lama::aint Lcompare(lama::aint, lama::aint);
lama::aint LflatCompare(lama::aint, lama::aint);
lama::aint LcompareTags(lama::aint, lama::aint);
lama::aint Lhash(lama::aint);
lama::aint Luppercase(lama::aint);
lama::aint Llowercase(lama::aint);
lama::aint LmakeArray(lama::aint length);
lama::aint LmakeString(lama::aint length);
lama::aint Lrandom(lama::aint);
lama::aint Ltime(void);
lama::aint LreadLine(void);
lama::aint LmatchSubString(lama::aint subject, lama::aint pattern, lama::aint position);
lama::aint Lregexp(lama::aint pattern);
lama::aint LregexpMatch(lama::aint regexp, lama::aint subject, lama::aint position);
lama::aint LstringInt(lama::aint);
lama::aint LgetEnv(lama::aint);
lama::aint Lsystem(lama::aint);

// Data
lama::aint Belem(lama::aint aggregate, lama::aint index);
lama::aint Bsta(lama::aint aggregate, lama::aint index, lama::aint value);
lama::aint Bstring(const char* const* args);
lama::aint Barray(lama::aint* args, lama::aint n);
lama::aint Bclosure(lama::aint* args, lama::aint bn);
lama::aint Bsexp(lama::aint* args, lama::aint n);
lama::aint LtagHash(const char* name);

// Pattern matching
lama::aint Btag(lama::aint value, lama::aint tag_hash, lama::aint n_fields);
lama::aint Barray_patt(lama::aint value, lama::aint n);
lama::aint Bstring_patt(lama::aint value, lama::aint string);
lama::aint Bstring_tag_patt(lama::aint value);
lama::aint Barray_tag_patt(lama::aint value);
lama::aint Bsexp_tag_patt(lama::aint value);
lama::aint Bclosure_tag_patt(lama::aint value);
lama::aint Bboxed_patt(lama::aint value);
lama::aint Bunboxed_patt(lama::aint value);

// Error handling
[[noreturn]] void Bmatch_failure(lama::aint value, const char* module, lama::aint line, lama::aint col);

} // extern "C"
