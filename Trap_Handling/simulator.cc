#include "simulator.h"

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include "elf_reader.h"
#include "five_stage_simulator.h"
#include "riscv.h"

Simulator::Simulator(const Options& opts)
    : single_step_(opts.single_step),
      verbose_(opts.verbose),
      dump_history_(opts.dump_history) {
  memory_ = std::make_unique<Memory>(opts.memory_size);
  LoadKernelImgToMemory(opts.kernel_img);

  auto elf_reader = ElfReader(opts.input_file, opts.verbose);
  elf_reader.LoadElfToMemory(memory_.get());
  pc_ = elf_reader.GetEntry();
  regs_.fill(0);

  uint32_t stack_size = opts.memory_size / 100;
  uint32_t stack_base = opts.memory_size - stack_size;
  IFDEF(DEBUG, assert(stack_base > stack_size));
  InitStack(stack_base, stack_size);
}

std::unique_ptr<Simulator> Simulator::Create(const Options& opts) {
  if (opts.pipeline_mode == "five-stage") {
    // for simplicity since base class shouldn't know its successor
    return std::make_unique<FiveStageSimulator>(opts);
  } else {
    // shouldn't reach here since abnormal inputs are handled when parsing
    std::cerr << std::format("Unknown pipeline mode {}\n", opts.pipeline_mode);
    exit(1);
  }
}

void Simulator::Panic(const char* format, ...) const {
  char buf[BUFSIZ];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  fprintf(stderr, "%s", buf);
  va_end(args);
  DumpHistory();
  fprintf(stderr, "Execution history in dump.txt\n");
  exit(1);
}

void Simulator::Panic(std::string_view str_view) const {
  std::cerr << str_view;
  DumpHistory();
  std::cerr << "Execution history in dump.txt\n";
  exit(1);
}

void Simulator::InitStack(uint32_t stack_base, uint32_t stack_size) {
  regs_[RISCV::REG_SP] = stack_base;
  stack_base_ = stack_base;
  stack_size_ = stack_size;
  for (uint32_t addr = stack_base; addr > stack_base - stack_size; addr--) {
    memory_->SetByte(addr, 0);
  }
}

void Simulator::LoadKernelImgToMemory(const std::string& kernel_img) {
  std::ifstream trap_file(kernel_img, std::ios::binary);
  if (!trap_file) {
    Panic(
        std::format("Loading kernel image error: {} not found\n", kernel_img));
    return;
  }
  std::vector<uint8_t> trap_code((std::istreambuf_iterator<char>(trap_file)),
                                 std::istreambuf_iterator<char>());

  uint32_t trap_base = 0x00020000;
  if (!memory_->CopyFrom(trap_code.data(), trap_base, trap_code.size())) {
    Panic("Invalid copy address\n");
  }
  if (verbose_) {
    std::cout << std::format("Loaded trap handler at {:#x}, size: {} bytes\n",
                             trap_base, trap_code.size());
  }
}
