#ifndef SRC_MEMORY_MANAGER_H
#define SRC_MEMORY_MANAGER_H

#include <memory>

#include "byte_addressable.h"
#include "options.h"
#include "cache.h"

// adaptor of byte addressable `backend_` as an interface for simulator
class MemoryManager {
  std::unique_ptr<ByteAddressable> backend_;
  Options opts_;
  
  // Task 1
  TieredCache* cache_backend_ = nullptr;

 public:
  explicit MemoryManager(const Options& opts);

  void CopyFrom(const void* src, uint32_t dest, uint32_t len);

  void SetByte(uint32_t addr, uint8_t val);
  void SetShort(uint32_t addr, uint16_t val);
  void SetInt(uint32_t addr, uint32_t val);
  void SetLong(uint32_t addr, uint64_t val);

  uint8_t GetByte(uint32_t addr) const;
  uint16_t GetShort(uint32_t addr) const;
  uint32_t GetInt(uint32_t addr) const;
  uint64_t GetLong(uint32_t addr) const;

  // Task 3
  uint32_t GetLastAccessLatency() const;

  // Task 2
  void Demote(uint32_t addr);

  // Task 4 
  void PrintStatistics() const;
};

#endif