#ifndef SRC_SIMULATOR_H
#define SRC_SIMULATOR_H

#include <array>
#include <memory>

#include "memory.h"
#include "options.h"
#include "riscv.h"

class Simulator {
 protected:
  uint64_t pc_ = 0;
  RISCV::Regs regs_{};
  uint32_t stack_base_ = 0;
  uint32_t stack_size_ = 0;
  std::unique_ptr<Memory> memory_ = nullptr;

  bool single_step_ = false;
  bool verbose_ = false;
  bool dump_history_ = false;

  void InitStack(uint32_t stack_base, uint32_t stack_size);
  virtual void DumpHistory() const {};
  void Panic(const char* format, ...) const;
  void Panic(std::string_view str_view) const;

  void LoadKernelImgToMemory(const std::string& kernel_img);

 public:
  explicit Simulator(const Options& opts);
  virtual ~Simulator() = default;

  virtual void Run() = 0;

  // factory method for future pipeline modes
  static std::unique_ptr<Simulator> Create(const Options& opts);
};

// forward declaration
class FiveStageSimulator;

#endif
