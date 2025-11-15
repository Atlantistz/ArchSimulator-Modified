#ifndef SRC_OPTIONS_
#define SRC_OPTIONS_

#include <CLI11/CLI11.hpp>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

inline static std::vector<std::string> pipeline_modes{"five-stage"};

enum class WritePolicy { WBWA };
enum class InclusionPolicy { Inclusive, Exclusive };
enum class ReplacementPolicy { LRU, Random };

// configuration for a single cache level
struct CacheLevelConfig {
  std::size_t size{32 * 1024};   // total cache size (bytes)
  std::size_t associativity{4};  // n-way associative
  std::size_t line_size{64};     // cache line size
  uint32_t latency{4};           // access latency (cycles)
  ReplacementPolicy replacement_policy{ReplacementPolicy::LRU};
};

struct Options {
  std::string input_file;
  std::string pipeline_mode = pipeline_modes[0];
  bool verbose = false;
  bool single_step = false;
  bool dump_history = false;
  uint32_t memory_size = 100 * 1024 * 1024;

  // cache configuration (global shared policies)
  bool enable_cache = false;
  WritePolicy write_policy = WritePolicy::WBWA;
  InclusionPolicy inclusion_policy = InclusionPolicy::Inclusive;
  std::vector<CacheLevelConfig> cache_levels;

  // latency simulation
  bool enable_latency = false;
  uint32_t memory_latency = 100;  // plain memory access latency

  // trace options
  bool enable_trace = false;
  std::string trace_output_file;

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

    // cache configuration options
    app.add_flag("--enable_cache", opts.enable_cache, "Enable cache hierarchy");

    // latency options
    app.add_flag("--enable_latency", opts.enable_latency,
                 "Enable latency simulation for cache and memory");
    app.add_option("--memory_latency", opts.memory_latency,
                   "Plain memory access latency in cycles")
        ->default_val(opts.memory_latency);

    // trace options
    app.add_flag("--enable_trace", opts.enable_trace, "Enable cache trace");
    app.add_option("--trace", opts.trace_output_file, "Cache trace output file")
        ->default_val("cache.trace");

    // cache policy options
    std::string write_policy_str = "wbwa";
    std::string inclusion_policy_str = "inclusive";
    std::map<std::string, WritePolicy> write_policy_map = {
        {"wbwa", WritePolicy::WBWA}};
    std::map<std::string, InclusionPolicy> inclusion_policy_map = {
        {"inclusive", InclusionPolicy::Inclusive},
        {"exclusive", InclusionPolicy::Exclusive}};
    std::map<std::string, ReplacementPolicy> replacement_policy_map = {
        {"lru", ReplacementPolicy::LRU}, {"random", ReplacementPolicy::Random}};

    // preset cache options
    std::vector<CacheLevelConfig> preset_cache_config = {
        {32 * 1024, 8, 64, 4, ReplacementPolicy::LRU},
        {256 * 1024, 8, 64, 10, ReplacementPolicy::LRU},
        {8 * 1024 * 1024, 16, 64, 40, ReplacementPolicy::LRU},
    };

    app.add_option("--write_policy", write_policy_str,
                   "Write policy: wbwa (write-back/write-allocate)")
        ->check(CLI::IsMember({"wbwa"}))
        ->default_val("wbwa");
    app.add_option("--inclusion_policy", inclusion_policy_str,
                   "Inclusion policy: inclusive, or exclusive")
        ->check(CLI::IsMember({"inclusive", "exclusive"}))
        ->default_val("inclusive");

    // cache level specification
    std::vector<std::string> cache_spec;
    app.add_option("--cache_levels", cache_spec,
                   "Cache levels specification: "
                   "size,assoc,linesize,latency,replacement_policy (e.g., "
                   "32K,8,64,4,lru for 32KB 8-way 64B-line 4-cycle lru cache). "
                   "Can specify multiple levels by repeating the option.")
        ->expected(0, 100);  // allow multiple levels

    // cache preset options
    std::string cache_preset = "none";
    app.add_option("--cache_preset", cache_preset,
                   "Cache preset: none, l1, l1l2, l1l2l3")
        ->default_val("none");

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
      exit(app.exit(e));
    }

    // after parsing
    // parse policies
    opts.write_policy = write_policy_map[write_policy_str];
    opts.inclusion_policy = inclusion_policy_map[inclusion_policy_str];
    // configure cache hierarchy based on preset or custom spec
    if (!cache_spec.empty()) {
      // customed cache specification
      opts.enable_cache = true;
      // parse each level:
      // "size,assoc,linesize,latency,replacement_policy"
      for (const auto& spec : cache_spec) {
        std::istringstream iss(spec);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(iss, token, ',')) {
          tokens.push_back(token);
        }
        if (tokens.size() != 5) {
          std::cerr << "Error: Invalid cache spec format: " << spec << "\n";
          std::cerr
              << "Expected: size,assoc,linesize,latency,replacement_policy\n";
          exit(1);
        }
        // parse size (with K/M suffix support)
        auto parse_size = [](const std::string& s) -> std::size_t {
          std::size_t val = std::stoull(s);
          if (s.back() == 'K' || s.back() == 'k') {
            val = std::stoull(s.substr(0, s.size() - 1)) * 1024;
          } else if (s.back() == 'M' || s.back() == 'm') {
            val = std::stoull(s.substr(0, s.size() - 1)) * 1024 * 1024;
          }
          return val;
        };

        std::size_t size = parse_size(tokens[0]);
        std::size_t assoc = std::stoull(tokens[1]);
        std::size_t linesize = std::stoull(tokens[2]);
        uint32_t latency = (tokens.size() == 4)
                               ? std::stoul(tokens[3])
                               : preset_cache_config[0].latency;
        if (!replacement_policy_map.contains(tokens[4])) {
          std::cerr << "Error: Invalid cache spec format: " << spec << "\n";
          std::cerr << tokens[4] << " not in supported replacement policies\n";
          exit(1);
        }
        auto replacement_policy = replacement_policy_map[tokens[4]];
        opts.cache_levels.emplace_back(size, assoc, linesize, latency,
                                       replacement_policy);
      }
    } else if (opts.enable_cache || cache_preset != "none") {
      opts.enable_cache = true;

      if (cache_preset == "l1") {
        opts.cache_levels.push_back(preset_cache_config[0]);
      } else if (cache_preset == "l1l2") {
        opts.cache_levels.push_back(preset_cache_config[0]);
        opts.cache_levels.push_back(preset_cache_config[1]);
      } else if (cache_preset == "l1l2l3") {
        opts.cache_levels.push_back(preset_cache_config[0]);
        opts.cache_levels.push_back(preset_cache_config[1]);
        opts.cache_levels.push_back(preset_cache_config[2]);
      }
    }

    return opts;
  }
};

#endif
