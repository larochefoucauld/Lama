// lamai: iterative interpreter for Lama bytecode.

#include "bytecode.hpp"
#include "vm.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

void usage(std::FILE* out) {
  std::fprintf(
      out,
      "Usage: lamai [options] <file.bc>\n"
      "Options:\n"
      "  -h, --help         Show this help message\n"
      "  -t, --trace        Enable execution tracing\n"
  );
}

} // namespace

int main(int argc, char** argv) {
  lama::VM::Options options;
  int i = 1;
  for (; i < argc && argv[i][0] == '-'; ++i) {
    if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
      usage(stdout);
      return 0;
    } else if (!std::strcmp(argv[i], "-t") || !std::strcmp(argv[i], "--trace")) {
      options.trace = true;
    } else {
      std::fprintf(stderr, "lamai: unknown option '%s'\n", argv[i]);
      usage(stderr);
      return 1;
    }
  }
  if (i >= argc) {
    std::fprintf(stderr, "lamai: no bytecode file specified\n");
    usage(stderr);
    return 1;
  }

  try {
    lama::Bytecode bc = lama::Bytecode::load(argv[i]);
    lama::VM(bc, options).run();
    return 0;
  } catch (const lama::BytecodeError& e) {
    std::fflush(stdout);
    std::fprintf(stderr, "lamai: error: %s\n", e.what());
    return 1;
  }
}
