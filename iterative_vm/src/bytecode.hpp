// Bytecode loading and checking

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lama {

class BytecodeError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// Little-endian bytecode reader
class Reader {
public:
  static_assert(std::endian::native == std::endian::little, "target byte order must match the one of bytecode");

  explicit Reader(std::span<const std::uint8_t> data, std::size_t pos = 0)
      : data_(data)
      , pos_(pos) {}

  std::uint8_t u8();
  std::int32_t i32();

  std::size_t pos() const {
    return pos_;
  }

  void seek(std::size_t pos) {
    pos_ = pos;
  }

  bool at_end() const {
    return pos_ >= data_.size();
  }

private:
  std::span<const std::uint8_t> data_;
  std::size_t pos_;
};

enum class PubKind : std::uint8_t {
  Function = 0,
  Global = 1
};

class Bytecode {
public:
  struct PublicSymbol {
    std::string_view name;
    std::int32_t code_offset;
    PubKind kind;
  };

  static Bytecode load(const std::string& path);

  Bytecode(Bytecode&&) = default;
  Bytecode& operator=(Bytecode&&) = default;
  // Copying requires re-computing the section ranges
  // as the data_ buffer is constructed from scratch.
  Bytecode(const Bytecode&) = delete;
  Bytecode& operator=(const Bytecode&) = delete;

  const std::string& name() const {
    return name_;
  }

  std::int32_t version() const {
    return version_;
  }

  std::size_t string_table_size() const {
    return string_table_.size();
  }

  std::size_t globals_count() const {
    return globals_count_;
  }

  std::span<const std::uint8_t> code() const {
    return code_;
  }

  // String table lookup
  std::string_view string_at(std::int32_t offset) const;

private:
  Bytecode() = default;

  std::vector<std::uint8_t> data_;
  std::string name_;
  std::int32_t version_ = 0;
  std::span<const std::uint8_t> string_table_;
  std::span<const std::uint8_t> code_;
  std::size_t globals_count_ = 0;
};

} // namespace lama
