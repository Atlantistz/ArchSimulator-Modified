#ifndef SRC_MEMORY_H
#define SRC_MEMORY_H

#include <elfio/elfio.hpp>
#include <span>
#include <vector>

#include "byte_addressable.h"

class Memory final : public ByteAddressable {
  std::vector<uint8_t> arena_;

  void CheckAddr(uint32_t addr, std::size_t len) const;
  void ReadSpan(uint32_t addr, std::span<uint8_t> out) override;
  void WriteSpan(uint32_t addr, std::span<const uint8_t> in) override;

 public:
  explicit Memory(std::size_t size) : arena_(size, 0) {}
};

#endif
