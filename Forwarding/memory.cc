#include "memory.h"

#include <cstdint>

#include "utils.h"

bool Memory::CopyFrom(const void *src, uint32_t dest, uint32_t len) {
  for (uint32_t i = 0; i < len; ++i) {
    if (!AddrExist(dest + i)) {
      IFDEF(DEBUG,
            fprintf(stderr, "Data copy to invalid addr 0x%x!\n", dest + i));
      return false;
    }
    SetByte(dest + i, ((uint8_t *)src)[i]);
  }
  return true;
}

bool Memory::SetByte(uint32_t addr, uint8_t val) {
  if (!AddrExist(addr)) {
    IFDEF(DEBUG, fprintf(stderr, "Byte write to invalid addr 0x%x!\n", addr));
    return false;
  }
  arena_[addr] = val;
  return true;
}

uint8_t Memory::GetByte(uint32_t addr) const {
  if (!AddrExist(addr)) {
    IFDEF(DEBUG, fprintf(stderr, "Byte read to invalid addr 0x%x!\n", addr));
    return false;
  }
  return arena_[addr];
}

bool Memory::SetShort(uint32_t addr, uint16_t val) {
  if (!AddrExist(addr)) {
    IFDEF(DEBUG, fprintf(stderr, "Short write to invalid addr 0x%x!\n", addr));
    return false;
  }
  SetByte(addr, val & 0xFF);
  SetByte(addr + 1, (val >> 8) & 0xFF);
  return true;
}

uint16_t Memory::GetShort(uint32_t addr) const {
  uint32_t b1 = GetByte(addr);
  uint32_t b2 = GetByte(addr + 1);
  return b1 + (b2 << 8);
}

bool Memory::SetInt(uint32_t addr, uint32_t val) {
  if (!AddrExist(addr)) {
    IFDEF(DEBUG, fprintf(stderr, "Int write to invalid addr 0x%x!\n", addr));
    return false;
  }
  SetByte(addr, val & 0xFF);
  SetByte(addr + 1, (val >> 8) & 0xFF);
  SetByte(addr + 2, (val >> 16) & 0xFF);
  SetByte(addr + 3, (val >> 24) & 0xFF);
  return true;
}

uint32_t Memory::GetInt(uint32_t addr) const {
  uint32_t b1 = GetByte(addr);
  uint32_t b2 = GetByte(addr + 1);
  uint32_t b3 = GetByte(addr + 2);
  uint32_t b4 = GetByte(addr + 3);
  return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
}

bool Memory::SetLong(uint32_t addr, uint64_t val) {
  if (!AddrExist(addr)) {
    IFDEF(DEBUG, fprintf(stderr, "Long write to invalid addr 0x%x!\n", addr));
    return false;
  }
  SetByte(addr, val & 0xFF);
  SetByte(addr + 1, (val >> 8) & 0xFF);
  SetByte(addr + 2, (val >> 16) & 0xFF);
  SetByte(addr + 3, (val >> 24) & 0xFF);
  SetByte(addr + 4, (val >> 32) & 0xFF);
  SetByte(addr + 5, (val >> 40) & 0xFF);
  SetByte(addr + 6, (val >> 48) & 0xFF);
  SetByte(addr + 7, (val >> 56) & 0xFF);
  return true;
}

uint64_t Memory::GetLong(uint32_t addr) const {
  uint64_t b1 = GetByte(addr);
  uint64_t b2 = GetByte(addr + 1);
  uint64_t b3 = GetByte(addr + 2);
  uint64_t b4 = GetByte(addr + 3);
  uint64_t b5 = GetByte(addr + 4);
  uint64_t b6 = GetByte(addr + 5);
  uint64_t b7 = GetByte(addr + 6);
  uint64_t b8 = GetByte(addr + 7);
  return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24) + (b5 << 32) + (b6 << 40) +
         (b7 << 48) + (b8 << 56);
}

bool Memory::AddrExist(uint32_t addr) const {
  if ((uint64_t)addr >= arena_.size()) return false;
  return true;
}
