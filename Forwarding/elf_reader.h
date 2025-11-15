#ifndef SRC_ELF_READER_H
#define SRC_ELF_READER_H

#include <cstdint>
#include <elfio/elfio.hpp>
#include <iostream>
#include <limits>
#include <string>

#include "memory.h"
#include "utils.h"

class ElfReader {
  ELFIO::elfio reader_;

 public:
  explicit ElfReader(const std::string& input_file, bool verbose = false) {
    if (!reader_.load(input_file)) {
      fprintf(stderr, "Fail to load ELF file %s!\n", input_file.c_str());
      exit(1);
    }
    if (verbose) {
      PrintElfInfo();
    }
  }
  ~ElfReader() = default;

  void LoadElfToMemory(Memory* memory) const {
    ELFIO::Elf_Half seg_num = reader_.segments.size();
    for (int i = 0; i < seg_num; ++i) {
      const ELFIO::segment* pseg = reader_.segments[i];

      uint64_t fullmemsz = pseg->get_memory_size();
      uint64_t fulladdr = pseg->get_virtual_address();
      // Our 32bit simulator cannot handle this
      if (fulladdr + fullmemsz > std::numeric_limits<uint32_t>::max()) {
        IFDEF(DEBUG, fprintf(stderr,
                             "ELF address space larger that 32bit! Set %d has "
                             "max addr of 0x%lx\n",
                             i, fulladdr + fullmemsz));
        exit(1);
      }

      uint32_t filesz = pseg->get_file_size();
      uint32_t memsz = pseg->get_memory_size();
      uint32_t addr = (uint32_t)pseg->get_virtual_address();

      for (uint32_t p = addr; p < addr + memsz; ++p) {
        if (p < addr + filesz) {
          memory->SetByte(p, pseg->get_data()[p - addr]);
        } else {
          memory->SetByte(p, 0);
        }
      }
    }
  }

  void PrintElfInfo() const {
    printf("==========ELF Information==========\n");

    if (reader_.get_class() == ELFCLASS32) {
      printf("Type: ELF32\n");
    } else {
      printf("Type: ELF64\n");
    }

    if (reader_.get_encoding() == ELFDATA2LSB) {
      printf("Encoding: Little Endian\n");
    } else {
      printf("Encoding: Large Endian\n");
    }

    if (reader_.get_machine() == EM_RISCV) {
      printf("ISA: RISC-V(0x%x)\n", reader_.get_machine());
    } else {
      IFDEF(DEBUG,
            fprintf(stderr, "ISA: Unsupported(0x%x)\n", reader_.get_machine()));
      exit(1);
    }

    ELFIO::Elf_Half sec_num = reader_.sections.size();
    printf("Number of Sections: %d\n", sec_num);
    printf("ID\tName\t\tAddress\tSize\n");

    for (int i = 0; i < sec_num; ++i) {
      const ELFIO::section* psec = reader_.sections[i];

      printf("[%d]\t%-12s\t0x%lx\t%ld\n", i, psec->get_name().c_str(),
             psec->get_address(), psec->get_size());
    }

    ELFIO::Elf_Half seg_num = reader_.segments.size();
    printf("Number of Segments: %d\n", seg_num);
    printf("ID\tFlags\tAddress\tFSize\tMSize\n");

    for (int i = 0; i < seg_num; ++i) {
      const ELFIO::segment* pseg = reader_.segments[i];

      printf("[%d]\t0x%x\t0x%lx\t%ld\t%ld\n", i, pseg->get_flags(),
             pseg->get_virtual_address(), pseg->get_file_size(),
             pseg->get_memory_size());
    }

    printf("===================================\n");
  }

  uint64_t GetEntry() const { return reader_.get_entry(); }
};

#endif
