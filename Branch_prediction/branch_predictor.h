#ifndef SRC_BRANCH_PREDICTOR_H
#define SRC_BRANCH_PREDICTOR_H

#include <cstdint>
#include <string>
#include <vector>

class BranchPredictor {
 public:
  virtual ~BranchPredictor() = default;
  virtual bool Predict(uint64_t pc) = 0;
  virtual void Update(uint64_t pc, bool taken, uint64_t target_pc) = 0;
  virtual std::string GetName() const = 0;
};

// Always Not Taken
class AlwaysNotTakenPredictor : public BranchPredictor {
 public:
  bool Predict(uint64_t pc) override { return false; }
  void Update(uint64_t pc, bool taken, uint64_t target_pc) override {
  }
  std::string GetName() const override { return "Always Not Taken"; }
};

// Always Taken
class AlwaysTakenPredictor : public BranchPredictor {
 public:
  bool Predict(uint64_t pc) override { return true; }
  void Update(uint64_t pc, bool taken, uint64_t target_pc) override {
  }
  std::string GetName() const override { return "Always Taken"; }
};

// 1-Bit predictor
class OneBitPredictor : public BranchPredictor {
  bool global_state_ = false;
 public:
  bool Predict(uint64_t pc) override { return global_state_; }
  void Update(uint64_t pc, bool taken, uint64_t target_pc) override {
    global_state_ = taken;
  }
  std::string GetName() const override { return "1-Bit"; }
};

// 2-Bit predictor
class TwoBitPredictor : public BranchPredictor {
  std::vector<uint8_t> bht_;
  size_t k_ = 0;

 public:
  explicit TwoBitPredictor(size_t k) : k_(k) {
    if (k_ == 0) k_ = 16;
    bht_.resize(k_, 2);

  bool Predict(uint64_t pc) override {
    uint32_t idx = pc % k_;
    uint8_t state = bht_[idx];
    return state >= 2;
  }

  void Update(uint64_t pc, bool taken, uint64_t target_pc) override {
    uint32_t idx = pc % k_;
    uint8_t state = bht_[idx];
    if (taken) {
      if (state < 3) state++;
    } else {
      if (state > 0) state--;
    }
    bht_[idx] = state;
  }
  std::string GetName() const override {
    return "2-Bit (K=" + std::to_string(k_) + ")";
  }
};

// Perceptron predictor
class PerceptronPredictor : public BranchPredictor {
 public:
  PerceptronPredictor() {
    // Init
  }

  bool Predict(uint64_t pc) override {
    return false;
  }

  void Update(uint64_t pc, bool taken, uint64_t target_pc) override {
  }

  std::string GetName() const override { return "Perceptron (Stub)"; }
};

#endif // SRC_BRANCH_PREDICTOR_H
