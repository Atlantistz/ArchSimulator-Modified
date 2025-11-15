#ifndef SRC_RISCV_H_
#define SRC_RISCV_H_

#include <array>
#include <cstdint>
#include <string>

#include "memory.h"

namespace RISCV {

static constexpr int REGNUM = 32;
using RegId = int;  // -1 means none
using Regs = std::array<uint64_t, REGNUM>;

enum Reg {
  REG_ZERO = 0,
  REG_RA = 1,
  REG_SP = 2,
  REG_GP = 3,
  REG_TP = 4,
  REG_T0 = 5,
  REG_T1 = 6,
  REG_T2 = 7,
  REG_S0 = 8,
  REG_S1 = 9,
  REG_A0 = 10,
  REG_A1 = 11,
  REG_A2 = 12,
  REG_A3 = 13,
  REG_A4 = 14,
  REG_A5 = 15,
  REG_A6 = 16,
  REG_A7 = 17,
  REG_S2 = 18,
  REG_S3 = 19,
  REG_S4 = 20,
  REG_S5 = 21,
  REG_S6 = 22,
  REG_S7 = 23,
  REG_S8 = 24,
  REG_S9 = 25,
  REG_S10 = 26,
  REG_S11 = 27,
  REG_T3 = 28,
  REG_T4 = 29,
  REG_T5 = 30,
  REG_T6 = 31,
};

inline const char* REGNAME[] = {
    "zero",  // x0
    "ra",    // x1
    "sp",    // x2
    "gp",    // x3
    "tp",    // x4
    "t0",    // x5
    "t1",    // x6
    "t2",    // x7
    "s0",    // x8
    "s1",    // x9
    "a0",    // x10
    "a1",    // x11
    "a2",    // x12
    "a3",    // x13
    "a4",    // x14
    "a5",    // x15
    "a6",    // x16
    "a7",    // x17
    "s2",    // x18
    "s3",    // x19
    "s4",    // x20
    "s5",    // x21
    "s6",    // x22
    "s7",    // x23
    "s8",    // x24
    "s9",    // x25
    "s10",   // x26
    "s11",   // x27
    "t3",    // x28
    "t4",    // x29
    "t5",    // x30
    "t6",    // x31
};

enum InstLargeType {
  R_TYPE,
  I_TYPE,
  S_TYPE,
  SB_TYPE,
  U_TYPE,
  UJ_TYPE,
};

enum InstType {
  UNKNOWN = -1,
  LUI = 0,
  AUIPC = 1,
  JAL = 2,
  JALR = 3,
  BEQ = 4,
  BNE = 5,
  BLT = 6,
  BGE = 7,
  BLTU = 8,
  BGEU = 9,
  LB = 10,
  LH = 11,
  LW = 12,
  LD = 13,
  LBU = 14,
  LHU = 15,
  SB = 16,
  SH = 17,
  SW = 18,
  SD = 19,
  ADDI = 20,
  SLTI = 21,
  SLTIU = 22,
  XORI = 23,
  ORI = 24,
  ANDI = 25,
  SLLI = 26,
  SRLI = 27,
  SRAI = 28,
  ADD = 29,
  SUB = 30,
  SLL = 31,
  SLT = 32,
  SLTU = 33,
  XOR = 34,
  SRL = 35,
  SRA = 36,
  OR = 37,
  AND = 38,
  ECALL = 39,
  SRET = 40,
  ADDIW = 41,
  MUL = 42,
  DIV = 43,
  REM = 44,
  LWU = 45,
  SLLIW = 46,
  SRLIW = 47,
  SRAIW = 48,
  ADDW = 49, 
  SUBW = 50, 
  SLLW = 51, 
  SRLW = 52, 
  SRAW = 53,
};

inline const char* INSTNAME[]{
    "lui",   "auipc", "jal",   "jalr",  "beq",   "bne",  "blt",  "bge",  "bltu",
    "bgeu",  "lb",    "lh",    "lw",    "ld",    "lbu",  "lhu",  "sb",   "sh",
    "sw",    "sd",    "addi",  "slti",  "sltiu", "xori", "ori",  "andi", "slli",
    "srli",  "srai",  "add",   "sub",   "sll",   "slt",  "sltu", "xor",  "srl",
    "sra",   "or",    "and",   "ecall", "sret",  "addiw", "mul",  "div",  "rem",  "lwu",
    "slliw", "srliw", "sraiw", "addw",  "subw",  "sllw", "srlw", "sraw",
};

// Opcode field
static constexpr int OP_REG = 0x33;
static constexpr int OP_IMM = 0x13;
static constexpr int OP_LUI = 0x37;
static constexpr int OP_BRANCH = 0x63;
static constexpr int OP_STORE = 0x23;
static constexpr int OP_LOAD = 0x03;
static constexpr int OP_SYSTEM = 0x73;
static constexpr int OP_AUIPC = 0x17;
static constexpr int OP_JAL = 0x6F;
static constexpr int OP_JALR = 0x67;
static constexpr int OP_IMM32 = 0x1B;
static constexpr int OP_32 = 0x3B;

inline bool IsBranch(InstType instType) {
  return instType == BEQ || instType == BNE || instType == BLT ||
         instType == BGE || instType == BLTU || instType == BGEU;
}

inline bool IsJump(InstType instType) {
  return instType == JAL || instType == JALR;
}

inline bool IsReadMem(InstType instType) {
  return instType == LB || instType == LH || instType == LW || instType == LD ||
         instType == LBU || instType == LHU || instType == LWU;
}

struct PipeOp {
  // fetch
  uint64_t pc = 0;
  uint32_t pc_len = 4;
  uint32_t inst = 0;

  // decode
  InstType inst_type = UNKNOWN;
  RegId rs1 = -1, rs2 = -1;
  int64_t op1 = 0, op2 = 0;
  RegId dest_reg = -1;
  int64_t offset = 0;
  // internal decode result
  std::string inst_str;

  // execute
  int64_t out = 0;
  bool write_mem = false;
  bool read_mem = false;
  bool read_sign_ext = false;
  uint32_t mem_len = 0;
  bool branch = false;
  uint32_t jump_pc = 0;
};

// simulator-irrelavant decoder and executor
void DecodeInst(PipeOp* op, const Regs& regs);
void ExecuteInst(PipeOp* op, bool* exit_ctrl, const Memory* mem);
}  // namespace RISCV

#endif
