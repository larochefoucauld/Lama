
// Compile-time checks that our redefined runtime API (runtime_api.hpp)
// is equivalent to origian C API (runtime_common.h)

#include "runtime_api.hpp"
#include "runtime_common.h"

#include <type_traits>

static_assert(std::is_same_v<aint, lama::aint>);
static_assert(lama::box(0) == BOX(0));
static_assert(lama::box(1) == BOX(1));
static_assert(lama::box(-1) == BOX(-1));
static_assert(lama::box(123456) == BOX(123456));
static_assert(lama::unbox(lama::box(-77)) == UNBOX(BOX(-77)));
static_assert(lama::unbox(lama::box(-77)) == -77);
static_assert(lama::is_unboxed(lama::box(5)) && !lama::is_unboxed(8));
