#include "memory.h"

#include <cstdint>
#include <format>
#include <stdexcept>

void Memory::CheckAddr(uint32_t addr, std::size_t len) const {
  if (addr + len >= arena_.size()) {
    throw std::runtime_error(std::format(
        "Invalid memory access to addr {:x} for len {}\n", addr, len));
  };
}

void Memory::ReadSpan(uint32_t addr, std::span<uint8_t> out) {
  CheckAddr(addr, out.size());
  std::memcpy(out.data(), arena_.data() + addr, out.size());
}

void Memory::WriteSpan(uint32_t addr, std::span<const uint8_t> in) {
  CheckAddr(addr, in.size());
  std::memcpy(arena_.data() + addr, in.data(), in.size());
}
