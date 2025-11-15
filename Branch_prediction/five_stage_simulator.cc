#include "five_stage_simulator.h"

#include <cassert>
#include <cstring>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "memory.h"
#include "options.h"
#include "riscv.h"
#include "simulator.h"
#include "utils.h"
#include "branch_predictor.h"

using namespace RISCV;

FiveStageSimulator::FiveStageSimulator(const Options& opts) : Simulator(opts) {
  if (opts.branch_predictor == "at") {
    predictor_ = std::make_unique<AlwaysTakenPredictor>();
  } else if (opts.branch_predictor == "1bit") {
    predictor_ = std::make_unique<OneBitPredictor>();
  } else if (opts.branch_predictor == "2bit") {
    predictor_ = std::make_unique<TwoBitPredictor>(opts.bht_size);
  } else if (opts.branch_predictor == "perceptron") {
    predictor_ = std::make_unique<PerceptronPredictor>();
  } else { // "nt"
    predictor_ = std::make_unique<AlwaysNotTakenPredictor>();
  }
  predictor_name_ = predictor_->GetName();
  history_.control_hazard_count = 0;
  history_.data_hazard_count = 0;
}

void FiveStageSimulator::Run() {
  // Main Simulation Loop
  while (true) {
    if (regs_[0] != 0) {
      Panic("Register 0's value is not zero!\n");
    }
    if (regs_[REG_SP] < stack_base_ - stack_size_) {
      Panic("Stack Overflow!\n");
    }

    // handle branch recoveries (mispredictions or predicted-taken)
    if (should_recover_branch_) {
      if (verbose_)
        printf("branch recovery: new pc 0x%08lx\n", branch_next_pc_);

      pc_ = branch_next_pc_;
      should_recover_branch_ = false;
      branch_next_pc_ = 0;

      wait_for_branch_ = false;
    }

    // clean data hazard
    wait_for_data_ = false;
    data_hazard_execute_op_dest_ = -1;
    data_hazard_mem_op_dest_ = -1;
    data_hazard_wb_op_dest_ = -1;

    // DO NOT CHANGE the execution order below
    WriteBack();
    MemoryAccess();
    Execute();
    Decode();
    Fetch();

    history_.cycle_count++;
    history_.reg_record.push_back(GetRegInfoStr());
    if (history_.reg_record.size() >= 100000) {  // Avoid using up memory
      history_.reg_record.clear();
      history_.inst_record.clear();
    }

    if (verbose_) {
      std::cout << GetRegInfoStr();
    }

    if (single_step_) {
      // printf("Type d to dump memory in dump.txt, press ENTER to continue: ");
      char ch;
      while ((ch = getchar()) != '\n') {
        if (ch == 'd') {
          DumpHistory();
        }
      }
    }
  }
}

void FiveStageSimulator::Fetch() {
  if (true == wait_for_branch_) {
   return;
  }

  /* if pipeline is stalled (our output slot is not empty), return */
  if (decode_op_ != nullptr) {
    if (verbose_) {
      printf("Fetch: stalled at fetch\n");
    }
    return;
  }

  if (pc_ % 2 != 0) {
    Panic("Illegal PC 0x%x!\n", pc_);
  }

  /* Allocate an op and send it down the pipeline. */
  auto op = std::make_unique<PipeOp>();
  op->inst = memory_->GetInt(pc_);
  if (verbose_) {
    printf("Fetched instruction 0x%.8x at address 0x%lx\n", op->inst, pc_);
  }
  op->pc = pc_;
  decode_op_ = std::move(op);

  pc_ += 4;
}

void FiveStageSimulator::Decode() {
  /* if no op to decode, return */
  if (decode_op_ == nullptr) {
    if (verbose_) {
      printf("Decode: Bubble\n");
    }
    return;
  }

  auto* op = decode_op_.get();
  if (op->pc_len != 4) {
    Panic(
        "Current implementation does not support 16bit RV64C instructions!\n");
  }
  try {
    DecodeInst(op, regs_);
  } catch (const std::exception& e) {
    Panic(e.what());
  }

  history_.inst_record.emplace_back(
      std::format("{:#010x}: {}\n", op->pc, op->inst_str));

  /* if downstream stall or occur data hazard, return
   * (and leave any input we had)
   */
  if (execute_op_ != nullptr) {
    if (verbose_) {
      std::cout << std::format(
          "Decoded instruction {:#010x} at address {:#x} as {}\n", op->inst,
          op->pc, op->inst_str);
    }
    return;
  }
  if (verbose_) {
    std::cout << std::format(
        "Decoded instruction {:#010x} at address {:#x} as {}\n", op->inst,
        op->pc, op->inst_str);
  }

  // data hazard detect at last to show inststr
  // detect and handle data hazard
  // data hazard
  auto wait_for_data = [&](RegId rs) -> bool {
    return rs > 0 &&
           (rs == data_hazard_execute_op_dest_ ||
            rs == data_hazard_mem_op_dest_ || rs == data_hazard_wb_op_dest_);
  };
  if (wait_for_data(op->rs1) || wait_for_data(op->rs2)) {
    if (verbose_) {
      printf("\tstalled at decode for data hazard\n");
    }
    // debug information
    history_.data_hazard_count++;
    return;
  }

  // Control Hazard
  if (IsBranch(op->inst_type)) {
    op->predicted_taken = predictor_->Predict(op->pc);

    if (verbose_) {
      printf("  Branch prediction: %s (%s)\n",
             op->predicted_taken ? "taken" : "not taken",
             predictor_name_.c_str());
    }

    if (op->predicted_taken) {
      // Predicted Taken
      wait_for_branch_ = true;
      
      PipeRecover(op->pc + op->offset);
      if (verbose_) {
        printf("Fetch: Bubble due to control hazard\n");
      }
    }

  } else if (IsJump(op->inst_type)) {
    wait_for_branch_ = true;
  }

  /* place op in downstream slot */
  execute_op_ = std::move(decode_op_);
  decode_op_ = nullptr;
}

void FiveStageSimulator::Execute() {
  /* if no op to execute, return */
  if (execute_op_ == nullptr) {
    if (verbose_) {
      printf("Execute: Bubble\n");
    }
    return;
  }
  auto* op = execute_op_.get();

  /* if downstream stall, return (and leave any input we had) */
  if (mem_op_ != nullptr) {
    if (verbose_) {
      printf("Execute: Stall\n");
    }
    return;
  }
  if (verbose_) {
    std::cout << std::format(
        "Execute instruction {:#010x} at address {:#x} as {}\n", op->inst,
        op->pc, op->inst_str);
  }
  history_.inst_count++;

  try {
    bool exit_ctrl = false;
    ExecuteInst(op, &exit_ctrl, memory_.get());
    if (exit_ctrl) {
      printf("Program exit from an exit() system call\n");
      if (dump_history_) {
        printf("Dumping history to dump.txt...");
        DumpHistory();
      }
      PrintStatistics();
      exit(0);
    }
  } catch (const std::exception& e) {
    Panic(e.what());
  }

  // Branch prediction verification

  bool actual_taken = op->branch;
  uint64_t actual_target_pc = op->jump_pc;
  uint64_t sequential_pc = op->pc + 4;

  if (IsBranch(op->inst_type)) {
    branch_count_++;
    predictor_->Update(op->pc, actual_taken, actual_target_pc);

    bool mispredicted = (op->predicted_taken != actual_taken);

    if (mispredicted) {
      mispredict_count_++;
      history_.control_hazard_count += 2;

      uint64_t correct_pc = actual_taken ? actual_target_pc : sequential_pc;
      PipeRecover(correct_pc);
      decode_op_.reset();

      wait_for_branch_ = true; 

      if (verbose_) {
        printf("  Branch prediction result: mispredicted\n");
        printf("Decode: Bubble due to control hazard\n");
        printf("Fetch: Bubble due to control hazard\n");
      }
    } else {
      if (verbose_) {
        printf("  Branch prediction result: correct\n");
      }
    }

  } else if (IsJump(op->inst_type)) {
    PipeRecover(actual_target_pc);
    decode_op_.reset();
  }
  // --- Lab 4 结束 ---


  // data hazard for decode to detect
  data_hazard_execute_op_dest_ = op->dest_reg;

  /* remove from upstream stage and place in downstream stage */
  mem_op_ = std::move(execute_op_);
  execute_op_ = nullptr;
}

void FiveStageSimulator::MemoryAccess() {
  /* if there is no instruction in this pipeline stage, we are done */
  if (!mem_op_) {
    if (verbose_) {
      printf("Memory Access: Bubble\n");
    }
    return;
  }

  PipeOp* op = mem_op_.get();

  const int64_t& op2 = op->op2;
  const RegId& dest_reg = op->dest_reg;
  int64_t& out = op->out;
  const bool write_mem = op->write_mem;
  const bool read_mem = op->read_mem;
  const bool read_sign_ext = op->read_sign_ext;
  const uint32_t mem_len = op->mem_len;

  if (verbose_) {
    std::cout << std::format(
        "MemoryAccess instruction {:#010x} at address {:#x} as {}\n", op->inst,
        op->pc, op->inst_str);
  }

  data_hazard_mem_op_dest_ = dest_reg;

  if (write_mem) {
    switch (mem_len) {
      case 1:
        memory_->SetByte(out, op2);
        break;
      case 2:
        memory_->SetShort(out, op2);
        break;
      case 4:
        memory_->SetInt(out, op2);
        break;
      case 8:
        memory_->SetLong(out, op2);
        break;
      default:
        Panic("Unknown memLen %d\n", mem_len);
    }
   }
  

  if (read_mem) {
    switch (mem_len) {
      case 1:
        if (read_sign_ext) {
          out = (int64_t)SEXT<8>(memory_->GetByte(out));
        } else {
          out = (uint64_t)memory_->GetByte(out);
        }
        break;
      case 2:
        if (read_sign_ext) {
          out = (int64_t)SEXT<16>(memory_->GetShort(out));
        } else {
          out = (uint64_t)memory_->GetShort(out);
        }
        break;
      case 4:
        if (read_sign_ext) {
          out = (int64_t)SEXT<32>(memory_->GetInt(out));
        } else {
          out = (uint64_t)memory_->GetInt(out);
        }
        break;
      case 8:
        if (read_sign_ext) {
          out = (int64_t)memory_->GetLong(out);
        } else {
          out = (uint64_t)memory_->GetLong(out);
        }
        break;
      default:
        Panic("Unknown memLen %d\n", mem_len);
    }
  }

  // data hazard for decode to detect
  data_hazard_mem_op_dest_ = dest_reg;

  /* clear stage input and transfer to next stage */
  wb_op_ = std::move(mem_op_);
  mem_op_ = nullptr;
}

void FiveStageSimulator::WriteBack() {
  /* if there is no instruction in this pipeline stage, we are done */
  if (!wb_op_) {
    if (verbose_) {
      printf("WriteBack: Bubble\n");
    }
    return;
  }
  // no situation to stall

  /* grab the op out of our input slot */
  PipeOp* op = wb_op_.get();
  if (verbose_) {
    std::cout << std::format(
        "WriteBack instruction {:#010x} at address {:#x} as {}\n", op->inst,
        op->pc, op->inst_str);
  }

  /* if this instruction writes a register, do so now */
  // Real Write Back
  const RegId& dest_reg = op->dest_reg;
  if (dest_reg > 0) {
    regs_[dest_reg] = op->out;
  }

  // data hazard for decode to detect
  data_hazard_wb_op_dest_ = dest_reg;

  /* free the op automatically with unique_ptr */
  wb_op_.reset();
  wb_op_ = nullptr;
}

void FiveStageSimulator::PipeRecover(uint32_t dest_pc) {
  /* if there is already a recovery scheduled, it must have come from a later
   * stage (which executes older instructions), hence that recovery overrides
   * our recovery. Simply return in this case. */

  /* schedule the recovery. This will be done once all pipeline stages simulate
   * the current cycle. */
  should_recover_branch_ = true;
  branch_next_pc_ = dest_pc;
}

void FiveStageSimulator::PrintStatistics() const {
  printf("------------ STATISTICS -----------\n");
  printf("Number of Instructions: %u\n", history_.inst_count);
  printf("Number of Cycles: %u\n", history_.cycle_count);
  printf("Avg Cycles per Instrcution: %.4f\n",
         (float)history_.cycle_count / history_.inst_count);
  printf("Number of Control Hazards: %u\n", history_.control_hazard_count);
  printf("Number of Data Hazards: %u\n", history_.data_hazard_count);
  
  double accuracy = 0.0;
  if (branch_count_ > 0) {
    accuracy = 1.0 - (double)mispredict_count_ / branch_count_;
  }
  printf("Branch Prediction Accuracy: %.4f (%s)\n", accuracy,
         predictor_name_.c_str());

  printf("-----------------------------------\n");
}

std::string FiveStageSimulator::GetRegInfoStr() const {
  std::string str = "------------ CPU STATE ------------\n";
  str += std::format("PC: {:#x}\n", pc_);
  for (uint32_t i = 0; i < 32; ++i) {
    str += std::format("{}: {:#018x}({}) ", REGNAME[i], regs_[i], regs_[i]);
    if (i % 4 == 3) {
      str += "\n";
    }
  }
  str += "-----------------------------------\n";
  return str;
}

void FiveStageSimulator::DumpHistory() const {
  std::ofstream ofile("dump.txt");
  ofile << "================== Excecution History =================="
        << std::endl;
  for (uint32_t i = 0; i < history_.inst_record.size(); ++i) {
    ofile << history_.inst_record[i];
    ofile << history_.reg_record[i];
  }
  ofile << "========================================================"
        << std::endl;
  ofile << std::endl;

  ofile.close();
}