// Virtual operand/call stack

#pragma once

#include "runtime_api.hpp"

#include <cstddef>

namespace lama {

// The stack is a contiguous mmap'ed memory region of a fixed size
// filled downwards, with a guard page mapped at the lower
// bound to prevent memory corruption upon overflow.
class Stack {
public:
  explicit Stack(std::size_t bytes);
  ~Stack();
  Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

  // one past the highest word
  aint* top() const {
    return top_;
  }

  // lowest usable word
  aint* limit() const {
    return limit_;
  }

private:
  void* base_;
  std::size_t total_;
  aint* top_;
  aint* limit_;
};

} // namespace lama
