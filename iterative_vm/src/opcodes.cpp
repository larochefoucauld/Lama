#include "opcodes.hpp"

namespace lama {

const char *op_name(std::uint8_t opcode) {
  switch (static_cast<Op>(opcode)) {
#define CASE(name, str) case Op::name: return str;
    CASE(Add, "ADD") CASE(Sub, "SUB") CASE(Mul, "MUL") CASE(Div, "DIV")
    CASE(Mod, "MOD") CASE(Lt, "LT") CASE(Le, "LE") CASE(Gt, "GT")
    CASE(Ge, "GE") CASE(Eq, "EQ") CASE(Ne, "NE") CASE(And, "AND")
    CASE(Or, "OR") CASE(Const, "CONST") CASE(String, "STRING")
    CASE(Sexp, "SEXP") CASE(Sta, "STA") CASE(Jmp, "JMP") CASE(End, "END")
    CASE(Drop, "DROP") CASE(Dup, "DUP") CASE(Swap, "SWAP") CASE(Elem, "ELEM")
    CASE(LdGlo, "LD_GLO") CASE(LdLoc, "LD_LOC") CASE(LdArg, "LD_ARG")
    CASE(LdClo, "LD_CLO") CASE(StGlo, "ST_GLO") CASE(StLoc, "ST_LOC")
    CASE(StArg, "ST_ARG") CASE(StClo, "ST_CLO") CASE(CjmpZ, "CJMP_Z")
    CASE(CjmpNz, "CJMP_NZ") CASE(Begin, "BEGIN")
    CASE(BeginClosure, "BEGIN_CLOSURE") CASE(Closure, "CLOSURE")
    CASE(Callc, "CALLC") CASE(Call, "CALL") CASE(Tag, "TAG")
    CASE(Array, "ARRAY") CASE(Fail, "FAIL") CASE(FailKeep, "FAIL_KEEP")
    CASE(Line, "LINE") CASE(PattStrCmp, "PATT_STR_CMP")
    CASE(PattString, "PATT_STRING") CASE(PattArray, "PATT_ARRAY")
    CASE(PattSexp, "PATT_SEXP") CASE(PattBoxed, "PATT_BOXED")
    CASE(PattUnboxed, "PATT_UNBOXED") CASE(PattClosure, "PATT_CLOSURE")
    CASE(Barray, "BARRAY") CASE(Eof, "EOF")
#undef CASE
  }
  return nullptr;
}

}  // namespace lama
