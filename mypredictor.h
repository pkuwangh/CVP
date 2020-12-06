// Author: Hao Wang (pkuwangh@gmail.com)

#ifndef __MY_PREDICTOR_H__
#define __MY_PREDICTOR_H__

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace implementation {

}

namespace analysis {

class DynInst {
 public:
  using Handle = std::shared_ptr<DynInst>;
  DynInst() = delete;
  DynInst(const DynInst&) = delete;
  DynInst& operator=(const DynInst&) = delete;
  ~DynInst() = default;
  DynInst(const uint64_t& pc,
      const uint32_t& piece,
      const bool& is_candidate,
      const uint32_t& cache_hit) {
    pc_ = pc;
    piece_ = piece;
    is_candidate_ = is_candidate;
    cache_hit_ = cache_hit;
  }

  void setMetaInfo(const uint32_t& inst_class,
      const uint64_t& src1,
      const uint64_t& src2,
      const uint64_t& src3,
      const uint64_t& dst) {
    inst_class_ = inst_class;
    dst_reg_ = dst;
    for (auto reg : {src1, src2, src3}) {
      src_regs_.push_back(reg);
    }
  }

  void setFinalInfo(const uint64_t& addr,
      const uint64_t& value,
      const uint64_t& latency) {
    addr_ = addr;
    value_ = value;
    latency_ = latency;
  }

  std::string getPcStr() const {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << pc_;
    ss << "." << std::hex << piece_;
    return ss.str();
  }

 private:
  uint64_t pc_ = 0xdeadbeef;
  uint32_t piece_ = 0;
  bool is_candidate_ = false;
  uint32_t cache_hit_ = 4;
  uint32_t inst_class_ = 8;
  uint64_t dst_reg_ = 0xdeadbeef;
  std::list<uint64_t> src_regs_;
  uint64_t addr_ = 0x0;
  uint64_t value_ = 0x0;
  uint64_t latency_ = 0;

  friend class ImemInst;
  friend class InstTracker;
};

class ImemInst {
 public:
  using Handle = std::shared_ptr<ImemInst>;
  ImemInst() = delete;
  ImemInst(const ImemInst&) = delete;
  ImemInst& operator=(const ImemInst&) = delete;
  ~ImemInst() = default;
  ImemInst(const DynInst::Handle& dynamic_inst);

  static void addValueToMap(
      std::unordered_map<uint64_t, uint64_t>& value_map,
      const uint64_t& value) {
    if (value_map.count(value) == 0) {
      value_map[value] = 0;
    }
    value_map[value] += 1;
  }

  void addDynInst(const DynInst::Handle& dynamic_inst);
  std::string getDisplayStr() const;

 private:
  std::string pc_str_;
  uint32_t inst_class_ = 8;
  bool is_candidate_ = false;
  uint64_t dst_reg_ = 0xdeadbeef;
  std::list<uint64_t> src_regs_;
  uint64_t count_ = 0;
  uint64_t total_latency_ = 0;
  uint64_t last_value_ = 0x0;
  std::unordered_map<uint64_t, uint64_t> cache_hit_map_;
  std::unordered_map<uint64_t, uint64_t> value_map_;
  std::unordered_map<uint64_t, uint64_t> stride_map_;

  friend class InstTracker;
};

class PatternRecorder {
 public:
  PatternRecorder() = delete;
  ~PatternRecorder() = default;
  PatternRecorder(const uint64_t& pc, const uint32_t& piece) :
    pc_ (pc),
    piece_ (piece)
  { }

  void addInsts(const uint64_t& pc, const uint32_t& piece, const uint64_t& value);
  void dumpPattern();

 private:
  const uint64_t pc_;
  const uint32_t piece_;
  std::list<uint64_t> values_;
};

class InstTracker {
 public:
  InstTracker() = delete;
  ~InstTracker() = default;
  InstTracker(const uint64_t& pc, const uint32_t& piece) {
    last_retired_seq_no_ = 0;
    total_count_ = 0;
    pattern_recorder_.reset(new PatternRecorder(pc, piece));
  }

  void addInstIssue(const uint64_t& seq_no,
      const uint64_t& pc,
      const uint32_t& piece,
      bool is_candidate,
      uint32_t cache_hit);

  void addInstExec(const uint64_t& seq_no,
      uint32_t inst_class,
      const uint64_t& src1,
      const uint64_t& src2,
      const uint64_t& src3,
      const uint64_t& dst);

  void addInstRetire(const uint64_t& seq_no,
      const uint64_t& addr,
      const uint64_t& value,
      const uint64_t& latency);

  void dumpImem();

  void dump() {
    dumpImem();
    pattern_recorder_->dumpPattern();
  }

 private:
  uint64_t last_retired_seq_no_ = 0;
  uint64_t total_count_ = 0;
  std::unordered_map<uint64_t, DynInst::Handle> inflight_insts_;
  std::map<std::string, ImemInst::Handle> tracked_insts_;
  std::unique_ptr<PatternRecorder> pattern_recorder_ = nullptr;
};

}

#endif
