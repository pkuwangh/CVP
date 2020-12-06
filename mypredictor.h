// Author: Hao Wang (pkuwangh@gmail.com)

#ifndef __MY_PREDICTOR_H__
#define __MY_PREDICTOR_H__

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace analysis {

class Utils {
 public:
  static std::string getPcStr(const uint64_t &pc, const uint32_t &piece)
  {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << pc;
    ss << "." << std::hex << piece;
    return ss.str();
  }

  static void incrConf(int32_t& conf, const int32_t& thd, int32_t val) {
    conf += val;
    if (conf > thd) {
      conf = thd;
    }
  }

  static void decrConf(int32_t& conf, const int32_t& thd, int32_t val) {
    conf -= val;
    if (conf < thd) {
      conf = thd;
    }
  }
};

class DynInst
{
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


namespace predictor {

class InflightEntry {
 public:
  InflightEntry() = default;
  void reset(const PredictionRequest& req) {
    eligible = req.is_candidate;
    pc = req.pc;
    piece = static_cast<uint32_t>(piece);
    prediction_result = 2;
    value = 0x0;
    single_stride_hit = false;
    single_stride_conf_hit = false;
    single_stride_mis_pred = false;
  }

  // stage-1
  bool eligible = false;
  uint64_t pc = 0x0;
  uint32_t piece = 0;
  // stage-2
  uint32_t prediction_result = 2;
  // stage-2
  uint64_t value = 0x0;
  // single-stride predictor
  bool single_stride_hit = false;
  bool single_stride_conf_hit = false;
  bool single_stride_mis_pred = false;
};

class BaseConfigs {
 public:
  const uint32_t ss_num_sets = 1;
  const uint32_t ss_num_ways = 1024;
  const uint32_t fms_num_sets = 1;
  const uint32_t fms_num_ways = 1024;
};

class SingleStrideEntry {
 public:
  using Handle = std::shared_ptr<SingleStrideEntry>;
  SingleStrideEntry(InflightEntry& op, const std::string& pc_to_watch);
  ~SingleStrideEntry() = default;

  static uint64_t calcTag(const uint64_t& pc, const uint32_t& piece);
  bool isTagMatch(const InflightEntry& op) const;

  std::string getDisplayStr() const;
  PredictionResult lookup(InflightEntry& req, uint32_t num_inflights);
  void specUpdate(InflightEntry& op);
  void finalUpdate(const InflightEntry& op);

 private:
  // configs
  const int32_t conf_min_ = 2;  // 0: invalid, 1: first value recorded
  const int32_t conf_thd_ = 8;
  const int32_t conf_max_ = 15;
  // real hardware
  int32_t confidence_ = 0;
  uint64_t tag_ = 0x0;
  uint64_t last_value_ = 0xdeadbeef;
  int64_t stride_ = 0xdeadbeef;
  uint32_t predict_ptr_ = 0;  // not faithfully modelled
  // debug
  std::string pc_str_;
  bool logging_enable_ = false;
};

class BasePredictor {
 public:
  BasePredictor(uint64_t pc, uint32_t piece) {
    pc_to_watch_ = analysis::Utils::getPcStr(pc, piece);
  }
  virtual ~BasePredictor() { }

  virtual PredictionResult lookup(InflightEntry& op, uint32_t num_inflights) = 0;
  virtual void specUpdate(InflightEntry& op) = 0;
  virtual void finalUpdate(const InflightEntry& op) = 0;

 private:
  std::string pc_to_watch_;
};

class SingleStridePredictor : public BasePredictor {
 public:
  SingleStridePredictor(const BaseConfigs& config, uint64_t pc, uint32_t piece);
  ~SingleStridePredictor() = default;

  uint32_t calcIndex(const InflightEntry& op) const;
  SingleStrideEntry::Handle findEntry(const InflightEntry& op);

  virtual PredictionResult lookup(InflightEntry& op, uint32_t num_inflights);
  virtual void specUpdate(InflightEntry& op);
  virtual void finalUpdate(const InflightEntry& op);

 private:
  const uint32_t num_sets_;
  const uint32_t num_ways_;
  std::vector<std::vector<SingleStrideEntry::Handle>> entries_;
  std::vector<uint32_t> rr_ptr_;
};

class StrideValue {
 public:
  StrideValue() = default;
  ~StrideValue() = default;

  uint64_t stride = 0;
  uint32_t count = 0;
  bool is_fuzzy = false;
};

class FuzzyMultiStrideEntry {
 public:
  using Handle = std::shared_ptr<FuzzyMultiStrideEntry>;
  FuzzyMultiStrideEntry();
  ~FuzzyMultiStrideEntry() = default;

 private:
  const uint32_t conf_min_ = 1;
  const uint32_t conf_max_ = 7;
  const uint32_t max_multi_stride_ = 4;
  std::vector<StrideValue> stride_count_;
};

class FuzzyMultiStridePredictor {
 public:
  FuzzyMultiStridePredictor(const BaseConfigs& config);
  ~FuzzyMultiStridePredictor() = default;

 private:
  const uint32_t num_sets_;
  const uint32_t num_ways_;
  std::vector<std::vector<FuzzyMultiStrideEntry::Handle>> entries_;
  std::vector<uint32_t> rr_ptr_;
};

}

// global structures
// ---------------- for analysis only ----------------
static std::unique_ptr<analysis::InstTracker> inst_tracker_ = nullptr;
// ---------------- real hardware ----------------
static std::unique_ptr<std::vector<predictor::InflightEntry>> inflight_ops_ = nullptr;

#endif
