cmake_minimum_required(VERSION 3.20)
project(iterative_vm C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Lama C runtime, linked statically.
set(RT ${CMAKE_CURRENT_SOURCE_DIR}/../runtime)
add_library(lama_runtime STATIC ${RT}/gc.c ${RT}/runtime.c ${RT}/printf.S)
target_compile_options(lama_runtime PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-std=c11 -O3 -g -fstack-protector-all -Wno-shift-negative-value>
  $<$<COMPILE_LANGUAGE:ASM>:-Wa,--noexecstack>)

add_executable(lamai
  src/main.cpp src/bytecode.cpp src/opcodes.cpp
  src/vm.cpp src/builtins.cpp src/api_check.cpp)
# Make runtime_common.h available only for api_check.cpp.
set_source_files_properties(src/api_check.cpp PROPERTIES INCLUDE_DIRECTORIES ${RT})
target_compile_options(lamai PRIVATE -Wall -Wextra)
target_link_libraries(lamai PRIVATE lama_runtime)

# Sanitizer build of the interpreter (the C runtime stays uninstrumented)
# cmake -S iterative_vm -B .build/cmake/sanitized_release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLAMAI_SANITIZE=ON
option(LAMAI_SANITIZE "Build lamai with AddressSanitizer and UBSan" OFF)
if(LAMAI_SANITIZE)
  target_compile_options(lamai PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -O1)
  target_link_options(lamai PRIVATE -fsanitize=address,undefined)
endif()
