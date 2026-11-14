#include "bytecode.hpp"

#include "opcodes.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace lama {

namespace {

constexpr std::size_t MagicSize = 4;
constexpr std::size_t HeaderSize = MagicSize + 5 * 4;
constexpr std::size_t ImportEntrySize = 4;
constexpr std::size_t PubEntrySize = 9;
constexpr std::int32_t Version = 1;

std::vector<std::uint8_t> read_file(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    throw BytecodeError("cannot open '" + path + "': " + std::strerror(errno));
  }
  std::vector<std::uint8_t> data;
  std::uint8_t buf[1 << 16];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
    data.insert(data.end(), buf, buf + n);
  }
  bool failed = std::ferror(f);
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
  if (data_.size() - std::min(pos_, data_.size()) < 4) {
    throw BytecodeError("truncated 32-bit operand at offset " + std::to_string(pos_));
  }
  std::uint32_t v = 0;
  for (std::size_t i = 0; i < 4; i++) {
    v |= static_cast<std::uint32_t>(data_[pos_ + i]) << (8 * i);
  }
  pos_ += 4;
  return static_cast<std::int32_t>(v);
}

std::string_view Bytecode::string_at(std::int32_t offset) const {
  if (offset < 0 || static_cast<std::size_t>(offset) >= string_table_.size()) {
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
  if (std::memcmp(all.data(), "LaMa", MagicSize) != 0) {
    throw BytecodeError("invalid magic number");
  }

  Reader r(all, MagicSize);
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

  Reader imports(all.subspan(imports_off, pubs_off - imports_off));
  for (std::int32_t i = 0; i < n_imports; i++) {
    bc.imports_.push_back(bc.string_at(imports.i32()));
  }
  Reader pubs(all.subspan(pubs_off, code_off - pubs_off));
  for (std::int32_t i = 0; i < n_pubs; i++) {
    std::string_view name = bc.string_at(pubs.i32());
    std::int32_t offset = pubs.i32();
    std::uint8_t flag = pubs.u8();
    if (flag > 1) {
      throw BytecodeError("invalid public symbol flag " + std::to_string(flag));
    }
    bc.public_syms_.push_back({name, offset, static_cast<PubKind>(flag)});
  }
  return bc;
}

} // namespace lama
