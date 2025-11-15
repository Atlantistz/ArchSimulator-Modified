#include "memory_manager.h"

#include "cache.h"
#include "memory.h"

MemoryManager::MemoryManager(const Options& opts) : opts_(opts) {
  // Task 1
  auto mem = std::make_unique<Memory>(opts.memory_size);

  if (opts.enable_cache) {
    auto cache = std::make_unique<TieredCache>(opts, std::move(mem));
    cache_backend_ = cache.get();
    backend_ = std::move(cache);
  } else {
    backend_ = std::move(mem);
  }
}

void MemoryManager::CopyFrom(const void* src, uint32_t dest, uint32_t len) {
  std::span<const uint8_t> data(reinterpret_cast<const uint8_t*>(src), len);
  backend_->WriteSpan(dest, data);
}

void MemoryManager::SetByte(uint32_t addr, uint8_t val) {
  backend_->Write(addr, val);
}

void MemoryManager::SetShort(uint32_t addr, uint16_t val) {
  backend_->Write(addr, val);
}

void MemoryManager::SetInt(uint32_t addr, uint32_t val) {
  backend_->Write(addr, val);
}

void MemoryManager::SetLong(uint32_t addr, uint64_t val) {
  backend_->Write(addr, val);
}

uint8_t MemoryManager::GetByte(uint32_t addr) const {
  uint8_t value{};
  backend_->Read(addr, value);
  return value;
}

uint16_t MemoryManager::GetShort(uint32_t addr) const {
  uint16_t value{};
  backend_->Read(addr, value);
  return value;
}

uint32_t MemoryManager::GetInt(uint32_t addr) const {
  uint32_t value{};
  backend_->Read(addr, value);
  return value;
}

uint64_t MemoryManager::GetLong(uint32_t addr) const {
  uint64_t value{};
  backend_->Read(addr, value);
  return value;
}

uint32_t MemoryManager::GetLastAccessLatency() const {
  // Task 3
  if (cache_backend_) {
    return cache_backend_->GetLastAccessLatency();
  }
  if (opts_.enable_latency) {
    return opts_.memory_latency;
  }
  return 1; // default
}

void MemoryManager::Demote(uint32_t addr) {
  // Task 2
  if (cache_backend_) {
    cache_backend_->Demote(addr);
  }
}

void MemoryManager::PrintStatistics() const {
  // Task 4
  if (cache_backend_) {
    cache_backend_->PrintStatistics();
  }
}