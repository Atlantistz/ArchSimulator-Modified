#ifndef SRC_BYTE_ADDRESSABLE_H
#define SRC_BYTE_ADDRESSABLE_H

#include <cstdint>
#include <cstring>
#include <span>

class ByteAddressable {
 public:
  virtual ~ByteAddressable() = default;
  virtual void ReadSpan(uint32_t addr, std::span<uint8_t> out) = 0;
  virtual void WriteSpan(uint32_t addr, std::span<const uint8_t> in) = 0;

  template <typename T>
  void Read(uint32_t addr, T& value) {
    std::array<uint8_t, sizeof(T)> buf{};
    ReadSpan(addr, std::span<uint8_t>(buf.data(), buf.size()));
    std::memcpy(&value, buf.data(), buf.size());
  }

  template <typename T>
  void Write(uint32_t addr, T value) {
    std::array<uint8_t, sizeof(T)> buf{};
    std::memcpy(buf.data(), &value, buf.size());
    WriteSpan(addr, std::span<const uint8_t>(buf.data(), buf.size()));
  }
};

#endif
