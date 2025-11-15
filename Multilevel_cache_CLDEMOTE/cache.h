#ifndef SRC_CACHE_H
#define SRC_CACHE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <span>
#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <format>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "byte_addressable.h"
#include "options.h"

inline uint32_t std_log2(uint64_t val) {
  if (val == 0) return 0;
  if (val == 1) return 0;
  uint64_t power = 1;
  uint32_t log = 0;
  while (power < val) {
    power *= 2;
    log++;
  }
  if (power != val) {
    throw std::runtime_error("Cache size/line size/associativity is not a power of 2");
  }
  return log;
}


// Task 4
struct CacheStats {
  uint64_t accesses = 0;
  uint64_t hits = 0;
  uint64_t misses = 0;
  uint64_t evictions = 0;
  uint64_t writebacks = 0;
};

// Task 1
struct CacheLine {
  bool valid = false;
  bool dirty = false;
  uint64_t tag = 0;
  std::vector<uint8_t> data;
  uint64_t lru_timestamp = 0;

  explicit CacheLine(size_t line_size) : data(line_size, 0) {}
};

// Task 1
class CacheSet {
 public:
  CacheSet(size_t associativity, size_t line_size, ReplacementPolicy policy)
      : assoc_(associativity), line_size_(line_size), replacement_policy_(policy) {
    lines_.resize(associativity, CacheLine(line_size));
  }

  CacheLine* Find(uint64_t tag) {
    for (auto& line : lines_) {
      if (line.valid && line.tag == tag) {
        return &line;
      }
    }
    return nullptr;
  }

  CacheLine* FindVictim(uint64_t current_cycle) {
    for (auto& line : lines_) {
      if (!line.valid) {
        return &line;
      }
    }

    if (replacement_policy_ == ReplacementPolicy::Random) {
      return &lines_[std::rand() % assoc_];
    } else { // LRU
      CacheLine* victim = &lines_[0];
      for (size_t i = 1; i < assoc_; ++i) {
        if (lines_[i].lru_timestamp < victim->lru_timestamp) {
          victim = &lines_[i];
        }
      }
      return victim;
    }
  }

  void UpdateLRU(CacheLine* line, uint64_t current_cycle) {
    if (replacement_policy_ == ReplacementPolicy::LRU) {
      line->lru_timestamp = current_cycle;
    }
  }

 private:
  size_t assoc_;
  size_t line_size_;
  ReplacementPolicy replacement_policy_;
  std::vector<CacheLine> lines_;
};

// Task 1
class CacheLevel {
 public:
  CacheLevel(const CacheLevelConfig& config, uint64_t* cycle_ptr)
      : config_(config), current_cycle_(cycle_ptr) {
    
    offset_bits_ = std_log2(config_.line_size);
    
    if (config_.associativity == 0 || config_.line_size == 0) {
        throw std::runtime_error("Cache associativity or line size cannot be zero.");
    }
    num_sets_ = config_.size / (config_.associativity * config_.line_size);
    if (num_sets_ == 0) {
        throw std::runtime_error("Cache size is too small for given parameters.");
    }
    index_bits_ = std_log2(num_sets_);
    
    tag_bits_ = 32 - index_bits_ - offset_bits_;

    sets_.resize(num_sets_, CacheSet(config_.associativity, config_.line_size, config_.replacement_policy));
  }

  CacheLine* Find(uint64_t addr, uint64_t* tag_out, uint64_t* index_out) {
    uint64_t index = GetIndex(addr);
    uint64_t tag = GetTag(addr);
    if (tag_out) *tag_out = tag;
    if (index_out) *index_out = index;
    return sets_[index].Find(tag);
  }


  CacheLine* Allocate(uint64_t addr, CacheLine** victim_line_out, uint64_t current_cycle) {
    uint64_t index = GetIndex(addr);
    uint64_t tag = GetTag(addr);

    CacheLine* victim = sets_[index].FindVictim(current_cycle);

    if (victim->valid) {
      *victim_line_out = new CacheLine(*victim);
    } else {
      *victim_line_out = nullptr; 
    }

    victim->valid = true;
    victim->dirty = false;
    victim->tag = tag;
    UpdateLRU(victim, current_cycle);
    
    return victim;
  }

  void UpdateLRU(CacheLine* line, uint64_t current_cycle) {
    if (config_.replacement_policy == ReplacementPolicy::LRU) {
      line->lru_timestamp = current_cycle;
    }
  }

  // Task 4
  CacheStats stats_;
  CacheLevelConfig config_;

  uint64_t num_sets_ = 0;
  uint32_t index_bits_ = 0;
  uint32_t offset_bits_ = 0;
  uint32_t tag_bits_ = 0;

  uint64_t GetTag(uint64_t addr) { return addr >> (index_bits_ + offset_bits_); }
  uint64_t GetIndex(uint64_t addr) { return (addr >> offset_bits_) & (num_sets_ - 1); }
  uint64_t GetOffset(uint64_t addr) { return addr & (config_.line_size - 1); }

  uint64_t GetAddr(uint64_t tag, uint64_t index) {
    return (tag << (index_bits_ + offset_bits_)) | (index << offset_bits_);
  }

 private:
  std::vector<CacheSet> sets_;
  uint64_t* current_cycle_;
};

// Task 1
class TieredCache : public ByteAddressable {
 public:
  TieredCache(const Options& opts, std::unique_ptr<ByteAddressable> main_memory)
      : opts_(opts), main_memory_(std::move(main_memory)), current_cycle_(0), last_access_latency_(0) {
    
    for (const auto& config : opts.cache_levels) {
      levels_.push_back(std::make_unique<CacheLevel>(config, &current_cycle_));
    }

    if (opts.enable_trace) {
      trace_file_.open(opts.trace_output_file);
    }
  }

  ~TieredCache() {
    if (opts_.enable_cache) {
      PrintStatistics();
    }
    if (trace_file_.is_open()) {
      trace_file_.close();
    }
  }

  void ReadSpan(uint32_t addr, std::span<uint8_t> out) override;
  void WriteSpan(uint32_t addr, std::span<const uint8_t> in) override;

  // Task 2
  void Demote(uint32_t addr);

  // Task 3
  uint32_t GetLastAccessLatency() const { return last_access_latency_; }

  // Task 4
  void PrintStatistics() const;

 private:  
  void HandleRead(size_t level_idx, uint64_t addr, std::span<uint8_t> out, uint32_t& latency, bool is_write_alloc = false);
  
  void HandleWrite(size_t level_idx, uint64_t addr, std::span<const uint8_t> in, uint32_t& latency);

  void Evict(size_t level_idx, CacheLine* victim_line, uint64_t victim_addr, uint32_t& latency);

  void BackInvalidate(int level_idx, uint64_t addr);

  void InvalidateInLowerLevels(size_t level_idx, uint64_t addr);

  void ReadFromMemory(uint64_t addr, std::span<uint8_t> out, uint32_t& latency);
  void WriteToMemory(uint64_t addr, std::span<const uint8_t> in, uint32_t& latency);

  // Task 4
  void Log(const std::string& message) {
    if (trace_file_.is_open()) {
      trace_file_ << std::format("[Cycle {}] ", current_cycle_) << message << std::endl;
    }
  }

  Options opts_;
  std::vector<std::unique_ptr<CacheLevel>> levels_;
  std::unique_ptr<ByteAddressable> main_memory_;

  uint64_t current_cycle_;
  uint32_t last_access_latency_;
  std::ofstream trace_file_;
};

#endif