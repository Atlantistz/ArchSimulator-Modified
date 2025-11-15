#ifndef SRC_FIVE_STAGE_SIMULATOR_
#define SRC_FIVE_STAGE_SIMULATOR_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "options.h"
#include "riscv.h"
#include "simulator.h"

class FiveStageSimulator final : public Simulator {
  // control hazard
  // wait_for_branch_ is signal for fetch stage to stall,
  // and need to turn to false when should_recover_branch_
  bool wait_for_branch_ = false;
  bool should_recover_branch_ = false;
  int64_t branch_next_pc_ = false;

  // data hazard
  // wait_for_data_ will be set only when no wait_for_branch_,
  // and is detected at every cycle
  bool wait_for_data_ = false;
  RISCV::RegId data_hazard_execute_op_dest_ = -1;
  RISCV::RegId data_hazard_mem_op_dest_ = -1;
  RISCV::RegId data_hazard_wb_op_dest_ = -1;

  std::unique_ptr<RISCV::PipeOp> decode_op_, execute_op_, mem_op_, wb_op_;

  struct History {
    uint32_t inst_count = 0;
    uint32_t cycle_count = 0;

    uint32_t data_hazard_count = 0;
    uint32_t control_hazard_count = 0;
    uint32_t memory_hazard_count = 0;

    std::vector<std::string> inst_record{};
    std::vector<std::string> reg_record{};
  } history_;

  void Fetch();
  void Decode();
  void Execute();
  void MemoryAccess();
  void WriteBack();

  // record jump pc and update pc next cycle
  void PipeRecover(uint32_t dest_pc);

  std::string GetRegInfoStr() const;
  void DumpHistory() const override;
  void PrintStatistics() const;

 public:
  explicit FiveStageSimulator(const Options& opts) : Simulator(opts) {};
  void Run() override;
};

#endif
