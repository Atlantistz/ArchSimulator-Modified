#include "riscv.h"

#include <array>
#include <cassert>
#include <format>
#include <stdexcept>

#include "memory.h"
#include "utils.h"

namespace RISCV {

int64_t HandleSystemCall(bool* exit_ctrl, int64_t type /* REG_A7 */,
                         int64_t arg1 /* REG_A0 */, const Memory* mem) {
  *exit_ctrl = false;
  switch (type) {
    case 0:  // print string
    {
      uint32_t addr = arg1;
      char ch = mem->GetByte(addr);
      while (ch != '\0') {
        printf("%c", ch);
        ch = mem->GetByte(++addr);
      }
      break;
    }
    case 1:  // print char
      printf("%c", (char)arg1);
      break;
    case 2:  // print num
      printf("%d", (int32_t)arg1);
      break;
    case 3:
    case 93:  // exit
      *exit_ctrl = true;
      break;
    case 4:  // read char
    {
      int r = scanf(" %c", (char*)&arg1);
      (void)r;  // ignore possible input error
      break;
    }
    case 5:  // read num
    {
      int r = scanf(" %ld", &arg1);
      (void)r;
      break;
    }
    case 6:  // print long long num
    {
      printf("%lld", (long long)arg1);
      break;
    }
    default:
      throw std::runtime_error(std::format("Unknown syscall type {}\n", type));
  }
  return arg1;
}

void DecodeInst(PipeOp* op, const Regs& regs) {
  std::string inst_name;
  std::string& inst_str = op->inst_str;
  std::string dest_str, op1_str, op2_str, offset_str;
  InstType& inst_type = op->inst_type;
  uint32_t inst = op->inst;
  // op1, op2 and offset are values
  int64_t &op1 = op->op1, &op2 = op->op2, &offset = op->offset;
  // reg1 and reg2 are operands
  RegId &dest_reg = op->dest_reg, &reg1 = op->rs1, &reg2 = op->rs2;

  uint32_t opcode = inst & 0x7F;
  uint32_t funct3 = (inst >> 12) & 0x7;
  uint32_t funct7 = (inst >> 25) & 0x7F;
  RegId rd = (inst >> 7) & 0x1F;
  RegId rs1 = (inst >> 15) & 0x1F;
  RegId rs2 = (inst >> 20) & 0x1F;
  int32_t imm_i = int32_t(inst) >> 20;
  int32_t imm_s =
      int32_t(((inst >> 7) & 0x1F) | ((inst >> 20) & 0xFE0)) << 20 >> 20;
  int32_t imm_sb = int32_t(((inst >> 7) & 0x1E) | ((inst >> 20) & 0x7E0) |
                           ((inst << 4) & 0x800) | ((inst >> 19) & 0x1000))
                       << 19 >>
                   19;
  int32_t imm_u = int32_t(inst) >> 12;
  int32_t imm_uj = int32_t(((inst >> 21) & 0x3FF) | ((inst >> 10) & 0x400) |
                           ((inst >> 1) & 0x7F800) | ((inst >> 12) & 0x80000))
                       << 12 >>
                   11;

  switch (opcode) {
    case OP_REG:
      op1 = regs[rs1];
      op2 = regs[rs2];
      reg1 = rs1;
      reg2 = rs2;
      dest_reg = rd;
      switch (funct3) {
        case 0x0:  // add, mul, sub
          if (funct7 == 0x00) {
            inst_type = ADD;
          } else if (funct7 == 0x01) {
            inst_type = MUL;
          } else if (funct7 == 0x20) {
            inst_type = SUB;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        case 0x1:  // sll
          if (funct7 == 0x00) {
            inst_type = SLL;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        case 0x2:  // slt
          if (funct7 == 0x00) {
            inst_type = SLT;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        case 0x3:  // sltu
          if (funct7 == 0x00) {
            inst_type = SLTU;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        case 0x4:  // xor div
          if (funct7 == 0x00) {
            inst_type = XOR;
          } else if (funct7 == 0x01) {
            inst_type = DIV;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        case 0x5:  // srl, sra
          if (funct7 == 0x00) {
            inst_type = SRL;
          } else if (funct7 == 0x20) {
            inst_type = SRA;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        case 0x6:  // or, rem
          if (funct7 == 0x00) {
            inst_type = OR;
          } else if (funct7 == 0x01) {
            inst_type = REM;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        case 0x7:  // and
          if (funct7 == 0x00) {
            inst_type = AND;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for funct3 {:#x}\n", funct7, funct3));
          }
          break;
        default:
          throw std::runtime_error(
              std::format("Unknown Funct3 field {:#x}\n", funct3));
      }
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = REGNAME[rs2];
      dest_str = REGNAME[rd];
      inst_str = inst_name + " " + dest_str + "," + op1_str + "," + op2_str;
      break;
    case OP_IMM:
      op1 = regs[rs1];
      reg1 = rs1;
      op2 = imm_i;
      dest_reg = rd;
      switch (funct3) {
        case 0x0:
          inst_type = ADDI;
          break;
        case 0x2:
          inst_type = SLTI;
          break;
        case 0x3:
          inst_type = SLTIU;
          break;
        case 0x4:
          inst_type = XORI;
          break;
        case 0x6:
          inst_type = ORI;
          break;
        case 0x7:
          inst_type = ANDI;
          break;
        case 0x1:
          inst_type = SLLI;
          op2 = op2 & 0x3F;
          break;
        case 0x5:
          if (((inst >> 26) & 0x3F) == 0x0) {
            inst_type = SRLI;
            op2 = op2 & 0x3F;
          } else if (((inst >> 26) & 0x3F) == 0x10) {
            inst_type = SRAI;
            op2 = op2 & 0x3F;
          } else {
            throw std::runtime_error(std::format(
                "Unknown funct7 {:#x} for OP_IMM\n", (inst >> 26) & 0x3F));
          }
          break;
        default:
          throw std::runtime_error(
              std::format("Unknown Funct3 field {:#x}\n", funct3));
      }
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = std::to_string(op2);
      dest_str = REGNAME[dest_reg];
      inst_str = inst_name + " " + dest_str + "," + op1_str + "," + op2_str;
      break;
    case OP_LUI:
      op1 = imm_u;
      op2 = 0;
      offset = imm_u;
      dest_reg = rd;
      inst_type = LUI;
      inst_name = INSTNAME[inst_type];
      op1_str = std::to_string(imm_u);
      dest_str = REGNAME[dest_reg];
      inst_str = inst_name + " " + dest_str + "," + op1_str;
      break;
    case OP_AUIPC:
      op1 = imm_u;
      op2 = 0;
      offset = imm_u;
      dest_reg = rd;
      inst_type = AUIPC;
      inst_name = INSTNAME[inst_type];
      op1_str = std::to_string(imm_u);
      dest_str = REGNAME[dest_reg];
      inst_str = inst_name + " " + dest_str + "," + op1_str;
      break;
    case OP_JAL:
      op1 = imm_uj;
      op2 = 0;
      offset = imm_uj;
      dest_reg = rd;
      inst_type = JAL;
      inst_name = INSTNAME[inst_type];
      op1_str = std::to_string(imm_uj);
      dest_str = REGNAME[dest_reg];
      inst_str = inst_name + " " + dest_str + "," + op1_str;
      break;
    case OP_JALR:
      op1 = regs[rs1];
      reg1 = rs1;
      op2 = imm_i;
      dest_reg = rd;
      inst_type = JALR;
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = std::to_string(op2);
      dest_str = REGNAME[dest_reg];
      inst_str = inst_name + " " + dest_str + "," + op1_str + "," + op2_str;
      break;
    case OP_BRANCH:
      op1 = regs[rs1];
      op2 = regs[rs2];
      reg1 = rs1;
      reg2 = rs2;
      offset = imm_sb;
      switch (funct3) {
        case 0x0:
          inst_type = BEQ;
          break;
        case 0x1:
          inst_type = BNE;
          break;
        case 0x4:
          inst_type = BLT;
          break;
        case 0x5:
          inst_type = BGE;
          break;
        case 0x6:
          inst_type = BLTU;
          break;
        case 0x7:
          inst_type = BGEU;
          break;
        default:
          throw std::runtime_error(
              std::format("Unknown funct3 {:#x} at OP_BRANCH\n", funct3));
      }
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = REGNAME[rs2];
      offset_str = std::to_string(offset);
      inst_str = inst_name + " " + op1_str + "," + op2_str + "," + offset_str;
      break;
    case OP_STORE:
      op1 = regs[rs1];
      op2 = regs[rs2];
      reg1 = rs1;
      reg2 = rs2;
      offset = imm_s;
      switch (funct3) {
        case 0x0:
          inst_type = SB;
          break;
        case 0x1:
          inst_type = SH;
          break;
        case 0x2:
          inst_type = SW;
          break;
        case 0x3:
          inst_type = SD;
          break;
        default:
          throw std::runtime_error(
              std::format("Unknown funct3 {:#x} for OP_STORE\n", funct3));
      }
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = REGNAME[rs2];
      offset_str = std::to_string(offset);
      inst_str =
          inst_name + " " + op2_str + "," + offset_str + "(" + op1_str + ")";
      break;
    case OP_LOAD:
      op1 = regs[rs1];
      reg1 = rs1;
      op2 = imm_i;
      offset = imm_i;
      dest_reg = rd;
      switch (funct3) {
        case 0x0:
          inst_type = LB;
          break;
        case 0x1:
          inst_type = LH;
          break;
        case 0x2:
          inst_type = LW;
          break;
        case 0x3:
          inst_type = LD;
          break;
        case 0x4:
          inst_type = LBU;
          break;
        case 0x5:
          inst_type = LHU;
          break;
        case 0x6:
          inst_type = LWU;
          break;
        default:
          throw std::runtime_error(
              std::format("Unknown funct3 {:#x} for OP_LOAD\n", funct3));
      }
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = std::to_string(op2);
      dest_str = REGNAME[rd];
      inst_str =
          inst_name + " " + dest_str + "," + op2_str + "(" + op1_str + ")";
      break;
    case OP_SYSTEM:
      if (funct3 == 0x0) {
        if (funct7 == 0x000) { // ECALL
          op1 = regs[REG_A0];
          op2 = regs[REG_A7];
          reg1 = REG_A0;
          reg2 = REG_A7;
          dest_reg = REG_A0;
          inst_type = ECALL;
          inst_name = INSTNAME[inst_type];
          inst_str = inst_name;
        } else if (funct7 == 0x08 && ((inst >> 20) & 0x1F) == 0x02) { // SRET
          inst_type = SRET;
          inst_name = "sret";
          inst_str = inst_name;
        } else {
          throw std::runtime_error(std::format(
              "Unknown OP_SYSTEM inst with funct3 {:#x} and funct7 {:#x}\n",
              funct3, funct7));
        }
      } else {
        throw std::runtime_error(std::format(
            "Unknown OP_SYSTEM inst with funct3 {:#x} and funct7 {:#x}\n",
            funct3, funct7));
      }
      break;
    case OP_IMM32:
      op1 = regs[rs1];
      reg1 = rs1;
      op2 = imm_i;
      dest_reg = rd;
      switch (funct3) {
        case 0x0:
          inst_type = ADDIW;
          break;
        case 0x1:
          inst_type = SLLIW;
          break;
        case 0x5:
          if (((inst >> 25) & 0x7F) == 0x0) {
            inst_type = SRLIW;
          } else if (((inst >> 25) & 0x7F) == 0x20) {
            inst_type = SRAIW;
          } else {
            throw std::runtime_error(std::format(
                "Unknown shift inst type {:#x}\n", ((inst >> 25) & 0x7F)));
          }
          break;
        default:
          throw std::runtime_error(
              std::format("Unknown funct3 {:#x} for OP_ADDIW\n", funct3));
      }
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = std::to_string(op2);
      dest_str = REGNAME[rd];
      inst_str = inst_name + " " + dest_str + "," + op1_str + "," + op2_str;
      break;
    case OP_32: {
      op1 = regs[rs1];
      op2 = regs[rs2];
      reg1 = rs1;
      reg2 = rs2;
      dest_reg = rd;

      uint32_t temp = (inst >> 25) & 0x7F;  // 32bit funct7 field
      switch (funct3) {
        case 0x0:
          if (temp == 0x0) {
            inst_type = ADDW;
          } else if (temp == 0x20) {
            inst_type = SUBW;
          } else {
            throw std::runtime_error(
                std::format("Unknown 32bit funct7 {:#x}\n", temp));
          }
          break;
        case 0x1:
          if (temp == 0x0) {
            inst_type = SLLW;
          } else {
            throw std::runtime_error(
                std::format("Unknown 32bit funct7 {:#x}\n", temp));
          }
          break;
        case 0x5:
          if (temp == 0x0) {
            inst_type = SRLW;
          } else if (temp == 0x20) {
            inst_type = SRAW;
          } else {
            throw std::runtime_error(
                std::format("Unknown 32bit funct7 {:#x}\n", temp));
          }
          break;
        default:
          throw std::runtime_error(
              std::format("Unknown 32bit funct3 {:#x}\n", funct3));
      }
      inst_name = INSTNAME[inst_type];
      op1_str = REGNAME[rs1];
      op2_str = REGNAME[rs2];
      dest_str = REGNAME[rd];
      inst_str = inst_name + " " + dest_str + "," + op1_str + "," + op2_str;
    } break;
    default:
      throw std::runtime_error(std::format(
          "Unsupported opcode {:#x} for inst {:#x}\n", opcode, inst));
  }
}

void ExecuteInst(PipeOp* op, bool* exit_ctrl, const Memory* mem) {
  const InstType inst_type = op->inst_type;
  const int64_t offset = op->offset;
  const int64_t pc = op->pc;

  int64_t& op1 = op->op1;
  int64_t& op2 = op->op2;
  int64_t& out = op->out;
  bool& write_mem = op->write_mem;
  bool& read_mem = op->read_mem;
  bool& read_sign_ext = op->read_sign_ext;
  uint32_t& mem_len = op->mem_len;

  // handle the branch change
  bool& branch = op->branch;
  uint32_t& jump_pc = op->jump_pc;
  jump_pc = pc + 4;

  switch (inst_type) {
    case LUI:
      out = offset << 12;
      break;
    case AUIPC:
      out = pc + (offset << 12);
      break;
    case JAL:
      out = pc + 4;
      jump_pc = pc + op1;
      branch = true;
      break;
    case JALR:
      out = pc + 4;
      jump_pc = (op1 + op2) & (~(uint64_t)1);
      branch = true;
      break;
    case BEQ:
      if (op1 == op2) {
        branch = true;
        jump_pc = pc + offset;
      }
      break;
    case BNE:
      if (op1 != op2) {
        branch = true;
        jump_pc = pc + offset;
      }
      break;
    case BLT:
      if (op1 < op2) {
        branch = true;
        jump_pc = pc + offset;
      }
      break;
    case BGE:
      if (op1 >= op2) {
        branch = true;
        jump_pc = pc + offset;
      }
      break;
    case BLTU:
      if ((uint64_t)op1 < (uint64_t)op2) {
        branch = true;
        jump_pc = pc + offset;
      }
      break;
    case BGEU:
      if ((uint64_t)op1 >= (uint64_t)op2) {
        branch = true;
        jump_pc = pc + offset;
      }
      break;
    case LB:
      read_mem = true;
      mem_len = 1;
      out = op1 + offset;
      read_sign_ext = true;
      break;
    case LH:
      read_mem = true;
      mem_len = 2;
      out = op1 + offset;
      read_sign_ext = true;
      break;
    case LW:
      read_mem = true;
      mem_len = 4;
      out = op1 + offset;
      read_sign_ext = true;
      break;
    case LD:
      read_mem = true;
      mem_len = 8;
      out = op1 + offset;
      read_sign_ext = true;
      break;
    case LBU:
      read_mem = true;
      mem_len = 1;
      out = op1 + offset;
      break;
    case LHU:
      read_mem = true;
      mem_len = 2;
      out = op1 + offset;
      break;
    case LWU:
      read_mem = true;
      mem_len = 4;
      out = op1 + offset;
      break;
    case SB:
      write_mem = true;
      mem_len = 1;
      out = op1 + offset;
      op2 = op2 & 0xFF;
      break;
    case SH:
      write_mem = true;
      mem_len = 2;
      out = op1 + offset;
      op2 = op2 & 0xFFFF;
      break;
    case SW:
      write_mem = true;
      mem_len = 4;
      out = op1 + offset;
      op2 = op2 & 0xFFFFFFFF;
      break;
    case SD:
      write_mem = true;
      mem_len = 8;
      out = op1 + offset;
      break;
    case ADDI:
    case ADD:
      out = op1 + op2;
      break;
    case ADDIW:
    case ADDW:
      out = (int64_t)((int32_t)op1 + (int32_t)op2);
      break;
    case SUB:
      out = op1 - op2;
      break;
    case SUBW:
      out = (int64_t)((int32_t)op1 - (int32_t)op2);
      break;
    case MUL:
      out = op1 * op2;
      break;
    case DIV:
      out = op1 / op2;
      break;
    case REM:
      out = op1 % op2;
      break;
    case SLTI:
    case SLT:
      out = op1 < op2 ? 1 : 0;
      break;
    case SLTIU:
    case SLTU:
      out = (uint64_t)op1 < (uint64_t)op2 ? 1 : 0;
      break;
    case XORI:
    case XOR:
      out = op1 ^ op2;
      break;
    case ORI:
    case OR:
      out = op1 | op2;
      break;
    case ANDI:
    case AND:
      out = op1 & op2;
      break;
    case SLLI:
    case SLL:
      out = op1 << op2;
      break;
    case SLLIW:
    case SLLW:
      out = int64_t(int32_t(op1 << op2));
      break;
    case SRLI:
    case SRL:
      out = (uint64_t)op1 >> (uint64_t)op2;
      break;
    case SRLIW:
    case SRLW:
      out = uint64_t(uint32_t((uint32_t)op1 >> (uint32_t)op2));
      break;
    case SRAI:
    case SRA:
      out = op1 >> op2;
      break;
    case SRAW:
    case SRAIW:
      out = int64_t(int32_t((int32_t)op1 >> (int32_t)op2));
      break;
    case ECALL: {
      const int64_t SYSCALL_FIND_MIN = 10;
      int64_t syscall_type = op2;

      if (syscall_type == SYSCALL_FIND_MIN) {
        op->dest_reg = 0; 
      } else {
        out = HandleSystemCall(exit_ctrl, op2, op1, mem);
      }
      break;
    }
    case SRET: {
      break;
    }
    default:
      throw std::runtime_error(std::format("Unknown instruction type: {}\n",
                                           static_cast<int>(inst_type)));
  }
}
};
