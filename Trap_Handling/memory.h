#ifndef SRC_MEMORY_MANAGER_H
#define SRC_MEMORY_MANAGER_H

#include <elfio/elfio.hpp>
#include <vector>

class Memory {
  std::vector<uint8_t> arena_{};
  bool AddrExist(uint32_t addr) const;

 public:
  explicit Memory(uint64_t memory_size) { arena_.resize(memory_size, 0); };
  ~Memory() = default;

  bool CopyFrom(const void *src, uint32_t dest, uint32_t len);

  bool SetByte(uint32_t addr, uint8_t val);
  uint8_t GetByte(uint32_t addr) const;

  bool SetShort(uint32_t addr, uint16_t val);
  uint16_t GetShort(uint32_t addr) const;

  bool SetInt(uint32_t addr, uint32_t val);
  uint32_t GetInt(uint32_t addr) const;

  bool SetLong(uint32_t addr, uint64_t val);
  uint64_t GetLong(uint32_t addr) const;
};

#endif