#include "bytecode.hpp"

#include "opcodes.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>

namespace lama {

namespace {

const char Magic[] = {'L', 'a', 'M', 'a'};
constexpr std::size_t HeaderSize = sizeof(Magic) + 5 * sizeof(std::int32_t);
constexpr std::size_t ImportEntrySize = sizeof(std::int32_t);
constexpr std::size_t PubEntrySize = 2 * sizeof(std::int32_t) + sizeof(std::uint8_t);
constexpr std::int32_t Version = 1;

std::vector<std::uint8_t> read_file(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    throw BytecodeError("cannot open '" + path + "': " + std::strerror(errno));
  }
  std::error_code ec;
  std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    std::fclose(f);
    throw BytecodeError("cannot stat '" + path + "': " + ec.message());
  }
  std::vector<std::uint8_t> data;
  data.resize(size);
  std::size_t n = std::fread(data.data(), 1, data.size(), f);
  bool failed = n != data.size();
  std::fclose(f);
  if (failed) {
    throw BytecodeError("error reading '" + path + "'");
  }
  return data;
}

} // namespace

std::uint8_t Reader::u8() {
  if (pos_ >= data_.size()) {
    throw BytecodeError("unexpected end of data at offset " + std::to_string(pos_));
  }
  return data_[pos_++];
}

std::int32_t Reader::i32() {
  std::int32_t v = 0;
  if (pos_ + sizeof(v) > data_.size()) {
    throw BytecodeError("truncated 32-bit operand at offset " + std::to_string(pos_));
  }
  std::memcpy(&v, &data_[pos_], sizeof(v));
  pos_ += sizeof(v);
  return static_cast<std::int32_t>(v);
}

std::string_view Bytecode::string_at(std::int32_t offset) const {
  if (static_cast<std::size_t>(offset) >= string_table_.size()) {
    throw BytecodeError("string table offset " + std::to_string(offset) + " out of range");
  }
  const auto* start = string_table_.data() + offset;
  std::size_t max = string_table_.size() - static_cast<std::size_t>(offset);
  const void* nul = std::memchr(start, 0, max);
  if (!nul) {
    throw BytecodeError("unterminated string at table offset " + std::to_string(offset));
  }
  return {
      reinterpret_cast<const char*>(start),
      static_cast<std::size_t>(static_cast<const std::uint8_t*>(nul) - start)
  };
}

Bytecode Bytecode::load(const std::string& path) {
  Bytecode bc;
  bc.data_ = read_file(path);
  bc.name_ = std::filesystem::path(path).stem().string();
  const std::span<const std::uint8_t> all(bc.data_);

  if (all.size() < HeaderSize) {
    throw BytecodeError("file too small for header (" + std::to_string(all.size()) + " bytes)");
  }
  if (std::memcmp(all.data(), Magic, sizeof(Magic)) != 0) {
    throw BytecodeError("invalid magic number");
  }

  Reader r(all, sizeof(Magic));
  bc.version_ = r.i32();
  std::int32_t strtab_size = r.i32();
  std::int32_t globals = r.i32();
  std::int32_t n_imports = r.i32();
  std::int32_t n_pubs = r.i32();

  if (bc.version_ != Version) {
    throw BytecodeError("unsupported bytecode version " + std::to_string(bc.version_));
  }
  if (strtab_size < 0 || globals < 0 || n_imports < 0 || n_pubs < 0) {
    throw BytecodeError("malformed header field");
  }

  const std::size_t imports_off = HeaderSize + static_cast<std::size_t>(strtab_size);
  const std::size_t pubs_off = imports_off + static_cast<std::size_t>(n_imports) * ImportEntrySize;
  const std::size_t code_off = pubs_off + static_cast<std::size_t>(n_pubs) * PubEntrySize;
  if (code_off >= all.size()) {
    throw BytecodeError(
        "sections exceed file size (code offset " + std::to_string(code_off) + ", file size " +
        std::to_string(all.size()) + ")"
    );
  }

  bc.string_table_ = all.subspan(HeaderSize, static_cast<std::size_t>(strtab_size));
  bc.code_ = all.subspan(code_off);
  bc.globals_count_ = static_cast<std::size_t>(globals);
  if (bc.code_.back() != static_cast<std::uint8_t>(Op::Eof)) {
    throw BytecodeError("bytecode must end with EOF opcode");
  }
  // TODO: imports

  return bc;
}

} // namespace lama
