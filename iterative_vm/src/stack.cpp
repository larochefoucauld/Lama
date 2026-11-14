#include "stack.hpp"

#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace lama {

Stack::Stack(std::size_t bytes) {
  const std::size_t guard = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
  total_ = bytes + guard;
  base_ = mmap(nullptr, total_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (base_ == MAP_FAILED) {
    throw std::runtime_error(std::string("cannot allocate VM stack: ") + std::strerror(errno));
  }
  if (mprotect(base_, guard, PROT_NONE) != 0) {
    throw std::runtime_error(std::string("cannot map VM stack guard page: ") + std::strerror(errno));
  }
  auto* bytes_base = static_cast<char*>(base_);
  limit_ = reinterpret_cast<aint*>(bytes_base + guard);
  top_ = reinterpret_cast<aint*>(bytes_base + total_);
}

Stack::~Stack() {
  munmap(base_, total_);
}

} // namespace lama
