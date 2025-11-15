#ifndef SRC_OPTIONS_
#define SRC_OPTIONS_

#include <CLI11/CLI11.hpp>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

inline static std::vector<std::string> pipeline_modes{"five-stage"};
inline static std::vector<std::string> predictor_modes{"nt", "at", "1bit", "2bit", "perceptron"};

struct Options {
  std::string input_file;
  std::string pipeline_mode = pipeline_modes[0];
  bool verbose = false;
  bool single_step = false;
  bool dump_history = false;
  uint32_t memory_size = 100 * 1024 * 1024;

  std::string branch_predictor = "nt";
  int bht_size = 16;

  static Options Parse(int argc, char** argv) {
    Options opts;

    CLI::App app{"RISC-V Simulator"};
    app.allow_extras(false);

    app.add_option("-i,--input", opts.input_file, "RISC-V ELF binary file")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_flag("-v,--verbose", opts.verbose, "Enable verbose output");
    app.add_flag("-s,--single_step", opts.single_step,
                 "Enable single-step execution");
    app.add_flag("-d,--dump_history", opts.dump_history,
                 "Dump execution history to dump.txt");
    app.add_option("--memory_size", opts.memory_size, "Memory size in bytes")
        ->default_val(opts.memory_size);
    app.add_option("--pipeline_mode", opts.pipeline_mode, "Pipeline mode")
        ->default_val(opts.pipeline_mode)
        ->check(CLI::IsMember(pipeline_modes));

    app.add_option("--bp_mode", opts.branch_predictor, "Branch predictor mode")
        ->default_val(opts.branch_predictor)
        ->check(CLI::IsMember(predictor_modes));
    app.add_option("--bht_size", opts.bht_size, "BHT size for 2-bit predictor (K)")
        ->default_val(opts.bht_size);

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
      exit(app.exit(e));
    }
    return opts;
  }
};

#endif