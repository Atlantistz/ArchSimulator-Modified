#include "cache.h"

void TieredCache::ReadSpan(uint32_t addr, std::span<uint8_t> out) {
  current_cycle_++;
  uint32_t latency = 0;
  last_access_latency_ = 0;

  if (levels_.empty()) {
    ReadFromMemory(addr, out, latency);
  } else {
    HandleRead(0, addr, out, latency);
  }

  last_access_latency_ = latency;
}

void TieredCache::WriteSpan(uint32_t addr, std::span<const uint8_t> in) {
  current_cycle_++;
  uint32_t latency = 0;
  last_access_latency_ = 0;

  if (levels_.empty()) {
    WriteToMemory(addr, in, latency);
  } else {
    HandleWrite(0, addr, in, latency);
  }
  
  last_access_latency_ = latency;
}

void TieredCache::HandleRead(size_t level_idx, uint64_t addr, std::span<uint8_t> out, uint32_t& latency, bool is_write_alloc) {
  CacheLevel* level = levels_[level_idx].get();
  
  uint64_t offset = level->GetOffset(addr);
  uint64_t remaining_in_line = level->config_.line_size - offset;

  if (out.size() > remaining_in_line) {
      HandleRead(level_idx, addr, out.subspan(0, remaining_in_line), latency, is_write_alloc);
      HandleRead(level_idx, addr + remaining_in_line, out.subspan(remaining_in_line), latency, is_write_alloc);
      return;
  }

  // Task 3
  latency += level->config_.latency;
  // Task 4
  level->stats_.accesses++;

  uint64_t tag, index;
  CacheLine* line = level->Find(addr, &tag, &index);

  if (line) {
    // Read Hit
    level->stats_.hits++;
    Log(std::format("L{} Read Hit: addr=0x{:x}", level_idx + 1, addr));
    
    level->UpdateLRU(line, current_cycle_);

    std::memcpy(out.data(), line->data.data() + offset, out.size());
    return;
  }

  // Read Miss
  level->stats_.misses++;
  Log(std::format("L{} Read Miss: addr=0x{:x}", level_idx + 1, addr));

  CacheLine* victim_line = nullptr;
  CacheLine* new_line = level->Allocate(addr, &victim_line, current_cycle_);
  uint64_t victim_addr = 0;

  if (victim_line) {
    victim_addr = level->GetAddr(victim_line->tag, index);
  }

  if (opts_.inclusion_policy == InclusionPolicy::Inclusive && victim_line) {
    Log(std::format("L{} Inclusive Back-Invalidate: addr=0x{:x}", level_idx + 1, victim_addr));
    BackInvalidate(level_idx - 1, victim_addr);
  }

  // Task 1
  if (victim_line) {
    Evict(level_idx, victim_line, victim_addr, latency);
    delete victim_line;
    victim_line = nullptr;
  }

  std::vector<uint8_t> line_buffer(level->config_.line_size);
  uint64_t line_addr = level->GetAddr(tag, index);

  if (level_idx + 1 < levels_.size()) {
    
    HandleRead(level_idx + 1, line_addr, std::span<uint8_t>(line_buffer.data(), line_buffer.size()), latency, is_write_alloc);
  } else {
    ReadFromMemory(line_addr, std::span<uint8_t>(line_buffer.data(), line_buffer.size()), latency);
  }

  std::memcpy(new_line->data.data(), line_buffer.data(), level->config_.line_size);
  new_line->valid = true;
  new_line->dirty = false;
  new_line->tag = tag;
  level->UpdateLRU(new_line, current_cycle_);

  // Exclusive
  if (opts_.inclusion_policy == InclusionPolicy::Exclusive && !is_write_alloc) {
      InvalidateInLowerLevels(level_idx + 1, line_addr);
  }

  std::memcpy(out.data(), new_line->data.data() + offset, out.size());
}


void TieredCache::HandleWrite(size_t level_idx, uint64_t addr, std::span<const uint8_t> in, uint32_t& latency) {
  CacheLevel* level = levels_[level_idx].get();
  
  uint64_t offset = level->GetOffset(addr);
  uint64_t remaining_in_line = level->config_.line_size - offset;
  if (in.size() > remaining_in_line) {
      HandleWrite(level_idx, addr, in.subspan(0, remaining_in_line), latency);
      HandleWrite(level_idx, addr + remaining_in_line, in.subspan(remaining_in_line), latency);
      return;
  }

  latency += level->config_.latency;
  level->stats_.accesses++;

  uint64_t tag, index;
  CacheLine* line = level->Find(addr, &tag, &index);

  if (line) {
    // Write Hit[WBWA]
    level->stats_.hits++;
    Log(std::format("L{} Write Hit: addr=0x{:x}", level_idx + 1, addr));
    
    level->UpdateLRU(line, current_cycle_);
    std::memcpy(line->data.data() + offset, in.data(), in.size());
    line->dirty = true;

    if (opts_.inclusion_policy == InclusionPolicy::Exclusive) {
        InvalidateInLowerLevels(level_idx + 1, level->GetAddr(tag, index));
    }
    return;
  }

  // Write Miss
  level->stats_.misses++;
  Log(std::format("L{} Write Miss: addr=0x{:x}", level_idx + 1, addr));

  // Task 1
  std::vector<uint8_t> dummy_out(in.size());
  HandleRead(level_idx, addr, std::span<uint8_t>(dummy_out.data(), dummy_out.size()), latency, true);

  line = level->Find(addr, &tag, &index);
  if (!line) {
    throw std::runtime_error("Cache logic error: Line not found after Write-Allocate");
  }

  Log(std::format("L{} Write-Allocate complete, performing write: addr=0x{:x}", level_idx + 1, addr));
  level->UpdateLRU(line, current_cycle_);
  std::memcpy(line->data.data() + offset, in.data(), in.size());
  line->dirty = true;
  
  // (Exclusive)
  if (opts_.inclusion_policy == InclusionPolicy::Exclusive) {
      InvalidateInLowerLevels(level_idx + 1, level->GetAddr(tag, index));
  }
}

void TieredCache::Evict(size_t level_idx, CacheLine* victim_line, uint64_t victim_addr, uint32_t& latency) {
  CacheLevel* level = levels_[level_idx].get();
  level->stats_.evictions++;
  Log(std::format("L{} Evict: addr=0x{:x} (Dirty={})", level_idx + 1, victim_addr, victim_line->dirty));

  if (victim_line->dirty) {
    level->stats_.writebacks++;
    Log(std::format("L{} Write-Back: addr=0x{:x}", level_idx + 1, victim_addr));
    
    std::span<const uint8_t> data_to_write(victim_line->data.data(), victim_line->data.size());

    if (level_idx + 1 < levels_.size()) {

      HandleWrite(level_idx + 1, victim_addr, data_to_write, latency);
    } else {

      WriteToMemory(victim_addr, data_to_write, latency);
    }

  }

  else if (opts_.inclusion_policy == InclusionPolicy::Exclusive) {
    if (level_idx + 1 < levels_.size()) {
      Log(std::format("L{} Exclusive Push-Down: addr=0x{:x}", level_idx + 1, victim_addr));

      std::span<const uint8_t> data_to_write(victim_line->data.data(), victim_line->data.size());
      HandleWrite(level_idx + 1, victim_addr, data_to_write, latency);
    }
  }
}


void TieredCache::BackInvalidate(int level_idx, uint64_t addr) {
  if (level_idx < 0) return;

  CacheLevel* level = levels_[level_idx].get();
  uint64_t tag, index;
  CacheLine* line = level->Find(addr, &tag, &index);

  if (line) {
    Log(std::format("L{} Back-Invalidated: addr=0x{:x}", level_idx + 1, addr));
    // (Inclusive)
    if (line->dirty) {
      uint32_t dummy_latency = 0;
      uint64_t victim_addr = level->GetAddr(tag, index);

      Evict(level_idx, line, victim_addr, dummy_latency);
    }
    line->valid = false;
  }

  BackInvalidate(level_idx - 1, addr);
}


void TieredCache::InvalidateInLowerLevels(size_t level_idx, uint64_t addr) {
  if (level_idx >= levels_.size()) return;

  CacheLevel* level = levels_[level_idx].get();
  CacheLine* line = level->Find(addr, nullptr, nullptr);

  if (line) {
    Log(std::format("L{} Exclusive Invalidate: addr=0x{:x}", level_idx + 1, addr));
    line->valid = false;
    line->dirty = false;
  }
  
  InvalidateInLowerLevels(level_idx + 1, addr);
}


// Task 2

void TieredCache::Demote(uint32_t addr) {
  if (levels_.empty()) return;

  current_cycle_++;
  Log(std::format("CLDEMOTE: addr=0x{:x}", addr));
  
  CacheLevel* l1 = levels_[0].get();
  uint64_t tag, index;
  CacheLine* line = l1->Find(addr, &tag, &index);

  if (!line) {
    Log("CLDEMOTE: L1 Miss, no action.");
    return;
  }

  uint64_t line_addr = l1->GetAddr(tag, index);
  uint32_t dummy_latency = 0; // demote

  if (opts_.inclusion_policy == InclusionPolicy::Inclusive) {
    // Inclusive。
    Log("CLDEMOTE: Inclusive policy, evicting from L1.");
    Evict(0, line, line_addr, dummy_latency);
    line->valid = false;
  } else {
    // Exclusive
    Log("CLDEMOTE: Exclusive policy, moving from L1 to L2.");
    Evict(0, line, line_addr, dummy_latency);
    line->valid = false;
  }
}

void TieredCache::ReadFromMemory(uint64_t addr, std::span<uint8_t> out, uint32_t& latency) {
  Log(std::format("Memory Read: addr=0x{:x}", addr));
  if (opts_.enable_latency) {
    latency += opts_.memory_latency;
  }
  main_memory_->ReadSpan(addr, out);
}

void TieredCache::WriteToMemory(uint64_t addr, std::span<const uint8_t> in, uint32_t& latency) {
  Log(std::format("Memory Write: addr=0x{:x}", addr));
  if (opts_.enable_latency) {
    latency += opts_.memory_latency;
  }
  main_memory_->WriteSpan(addr, in);
}


void TieredCache::PrintStatistics() const {
  std::cout << "---------- CACHE STATISTICS ----------" << std::endl;
  std::cout << std::format("Global Policies: Inclusion={}, Write={}\n",
        (opts_.inclusion_policy == InclusionPolicy::Inclusive ? "Inclusive" : "Exclusive"),
        (opts_.write_policy == WritePolicy::WBWA ? "WBWA" : "Unknown")
  );

  for (size_t i = 0; i < levels_.size(); ++i) {
        const auto& level = levels_[i];
        const auto& stats = level->stats_;
        double hit_rate = (stats.accesses == 0) ? 0.0 : (double)stats.hits / stats.accesses;
        
        std::cout << std::format("L{} Cache ({}B, {}-way, {}B line, {} cycles, {})\n",
            i + 1,
            level->config_.size,
            level->config_.associativity,
            level->config_.line_size,
            level->config_.latency,
            (level->config_.replacement_policy == ReplacementPolicy::LRU ? "LRU" : "Random")
        );
        std::cout << std::format(
            "\tAccesses: {}\n\tHits: {}\n\tMisses: {}\n\tHit Rate: {:.2f}%\n",
            stats.accesses, stats.hits, stats.misses, hit_rate * 100
        );
        std::cout << std::format(
            "\tEvictions: {}\n\tWritebacks: {}\n",
            stats.evictions, stats.writebacks
        );
  }
  std::cout << "--------------------------------------" << std::endl;
}