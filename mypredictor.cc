#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "cvp.h"
#include "mypredictor.h"

namespace analysis {

ImemInst::ImemInst(const DynInst::Handle& dynamic_inst) {
  pc_str_ = Utils::getPcStr(dynamic_inst->pc_, dynamic_inst->piece_);
  inst_class_ = dynamic_inst->inst_class_;
  is_candidate_ = dynamic_inst->is_candidate_;
  dst_reg_ = dynamic_inst->dst_reg_;
  src_regs_ = dynamic_inst->src_regs_;
  count_ = 0;
  total_latency_ = 0;
  last_value_ = 0xdeadbeef;
}

void ImemInst::addDynInst(const DynInst::Handle& dynamic_inst) {
  count_ += 1;
  total_latency_ += dynamic_inst->latency_;
  if (is_candidate_) {
    addValueToMap(cache_hit_map_, dynamic_inst->cache_hit_);
    addValueToMap(value_map_, dynamic_inst->value_);
    if (last_value_ != 0xdeadbeef) {
      const auto stride = dynamic_inst->value_ - last_value_;
      addValueToMap(stride_map_, stride);
    }
    last_value_ = dynamic_inst->value_;
  }
}

std::string ImemInst::getDisplayStr() const {
  // lambda to return a string of N whitespaces
  auto getSpaceStr = [](int n) -> std::string {
    std::string res = std::string(n, ' ');
    return res;
  };
  std::stringstream ss;
  ss << pc_str_ << getSpaceStr(2);
  ss << std::setw(4) << std::left;
  switch (inst_class_) {
    case 0: ss << "alu"; break;
    case 1: ss << "ld"; break;
    case 2: ss << "st"; break;
    case 3: ss << "br"; break;
    case 4: ss << "jmp"; break;
    case 5: ss << "jr"; break;
    case 6: ss << "fp"; break;
    case 7: ss << "mul"; break;
    default: ss << "???"; break;
  }
  if (dst_reg_ != 0xdeadbeef) {
    ss << "r" << std::setw(2) << std::left << dst_reg_ << " <- ";
  } else {
    ss << getSpaceStr(7);
  }
  for (const auto& reg : src_regs_) {
    if (reg != 0xdeadbeef) {
      std::stringstream rss;
      rss << "r" << reg << ",";
      ss << std::setw(4) << std::left << rss.str();
    } else {
      ss << getSpaceStr(4);
    }
  }
  ss << std::setw(6) << std::right << (total_latency_ / count_);
  // stride & value pattern
  using pattern_dist = std::unordered_map<uint64_t, uint64_t>;
  auto get_pattern = [this](const pattern_dist& dist, bool stride) -> std::string {
    using pattern_item = std::pair<uint64_t, uint64_t>;
    std::vector<pattern_item> patterns(dist.begin(), dist.end());
    std::sort(
      patterns.begin(), patterns.end(),
      [](const pattern_item& a, const pattern_item& b) -> bool {
        return a.second > b.second;
      }
    );
    std::stringstream pss;
    const uint32_t top_n = 4;
    uint64_t eff_count = stride ? (count_ - 1) : count_;
    for (size_t i = 0; i < top_n; ++i) {
      if (i < patterns.size()) {
        if (stride) {
          const int64_t stride_val = static_cast<int64_t>(patterns[i].first);
          if (stride_val < 0) {
            pss << "-0x" << std::hex << (-stride_val) << "/";
          } else {
            pss << "0x" << std::hex << stride_val << "/";
          }
        } else {
          pss << "0x" << std::hex << patterns[i].first << "/";
        }
        pss << std::dec << patterns[i].second << "/";
        pss << (100 * patterns[i].second / eff_count) << "%";
      } else {
        pss << "-";
      }
      if (i < top_n - 1) {
        pss << ", ";
      }
    }
    return pss.str();
  };
  ss << getSpaceStr(2) << "[" << get_pattern(value_map_, false) << "]";
  ss << getSpaceStr(2) << "[" << get_pattern(stride_map_, true) << "]";
  return ss.str();
}

void InstTracker::addInstIssue(const uint64_t& seq_no,
    const uint64_t& pc,
    const uint32_t& piece,
    bool is_candidate,
    uint32_t cache_hit) {
  if (inflight_insts_.count(seq_no) > 0) {
    std::cout << "seqnum=" << seq_no << " again???" << std::endl;
  } else {
    DynInst::Handle dynamic_inst = std::make_shared<DynInst>(
        pc, piece, is_candidate, cache_hit);
    inflight_insts_[seq_no] = dynamic_inst;
  }
}

void InstTracker::addInstExec(const uint64_t& seq_no,
    uint32_t inst_class,
    const uint64_t& src1,
    const uint64_t& src2,
    const uint64_t& src3,
    const uint64_t& dst) {
  if (inflight_insts_.count(seq_no) == 0) {
    std::cout << "seqnum=" << seq_no << " gone???" << std::endl;
  } else {
    inflight_insts_[seq_no]->setMetaInfo(inst_class, src1, src2, src3, dst);
  }
}

void InstTracker::addInstRetire(const uint64_t& seq_no,
    const uint64_t& addr,
    const uint64_t& value,
    const uint64_t& latency) {
  total_count_ += 1;
  if (inflight_insts_.count(seq_no) == 0) {
    std::cout << "seqnum=" << seq_no << " gone at retire???" << std::endl;
  } else {
    auto& dynamic_inst = inflight_insts_[seq_no];
    dynamic_inst->setFinalInfo(addr, value, latency);
    const std::string pc_str = Utils::getPcStr(dynamic_inst->pc_, dynamic_inst->piece_);
    if (tracked_insts_.count(pc_str) == 0) {
      ImemInst::Handle imem_inst = std::make_shared<ImemInst>(dynamic_inst);
      tracked_insts_[pc_str] = imem_inst;
    }
    tracked_insts_[pc_str]->addDynInst(dynamic_inst);
    pattern_recorder_->addInsts(dynamic_inst->pc_, dynamic_inst->piece_, value);
  }
  // remove flushed insts
  for (uint64_t i = last_retired_seq_no_; i < seq_no; ++i) {
    if (inflight_insts_.count(i) > 0) {
      inflight_insts_.erase(i);
    }
  }
  last_retired_seq_no_ = seq_no;
}

void InstTracker::dumpImem() {
  uint64_t run_count = 0;
  uint32_t skipped_lines = 0;
  bool skipping = false;
  std::cout << "================ IMEM Start ================" << std::endl;
  for (const auto& x : tracked_insts_) {
    const ImemInst::Handle& inst = x.second;
    run_count += inst->count_;
    const float weight = 100.0 * inst->count_ / total_count_;
    const float run_weight = 100.0 * run_count / total_count_;
    if (weight < 0.01) {
      if (!skipping) {
        skipping = true;
      }
      skipped_lines += 1;
      continue;
    } else {
      if (skipping) {
        std::cout << " ... skipped " << skipped_lines << " lines." << std::endl;
        skipped_lines = 0;
        skipping = false;
      }
    }
    std::cout << std::setw(8) << inst->count_;
    std::cout << std::fixed;
    std::cout << std::setw(7) << std::setprecision(3) << weight << "%";
    std::cout << std::setw(7) << std::setprecision(2) << run_weight << "%";
    std::cout << std::defaultfloat << "  ";
    std::cout << inst->getDisplayStr() << std::endl;
  }
  std::cout << "================ IMEM End ================" << std::endl;
}


void PatternRecorder::addInsts(
  const uint64_t& pc, const uint32_t& piece, const uint64_t& value) {
  if (pc_ == pc && piece_ == piece) {
    values_.push_back(value);
  }
}

void PatternRecorder::dumpPattern() {
  uint64_t last_v = 0xdeadbeef;
  int64_t last_s = 0xdeadbeef;
  int64_t curr_s = 0xdeadbeef;
  uint64_t stride_start = 0xdeadbeef;
  uint32_t stride_repeat = 0;
  auto print_stride = [&last_s, &stride_start, &stride_repeat]() -> void {
    if (stride_repeat > 0) {
      std::cout << "0x" << std::hex << std::left << std::setw(16);
      std::cout << std::setfill('0') << stride_start << std::setfill(' ');
      if (last_s < 0) {
        std::cout << "\t-0x" << std::setw(19) << std::left << (-last_s);
      } else {
        std::cout << "\t0x" << std::setw(20) << std::left << last_s;
      }
      std::cout << std::dec << std::setw(6) << std::right << stride_repeat;
      std::cout << std::endl;
    }
  };
  std::cout << "================ Pattern Start ================" << std::endl;
  std::cout << "pc = 0x" << std::hex << pc_;
  std::cout << ", piece = " << std::dec << piece_ << std::endl;
  for (const auto& v : values_) {
    if (last_v != 0xdeadbeef) {
      curr_s = static_cast<int64_t>(v - last_v);
      if (last_s != 0xdeadbeef && last_s == curr_s) {
        stride_repeat += 1;
      } else {
        if (last_s != 0xdeadbeef) {
          print_stride();
        } else {
          std::cout << "2nd appearance: value=0x" << std::hex << v << std::endl;
        }
        stride_start = v;
        stride_repeat = 1;
      }
      last_s = curr_s;
    } else {
      std::cout << "1st appearance: value=0x" << std::hex << v << std::endl;
    }
    last_v = v;
  }
  print_stride();
  std::cout << "================ Pattern End ================" << std::endl;
}

}


namespace predictor {

uint64_t SingleStrideEntry::calcTag(const uint64_t& pc, const uint32_t& piece) {
  return pc + static_cast<uint64_t>(piece);
}

bool SingleStrideEntry::isTagMatch(const InflightEntry& op) const {
  return (confidence_ > 0 && tag_ == calcTag(op.pc, op.piece));
}

std::string SingleStrideEntry::getDisplayStr() const {
  std::stringstream ss;
  ss << pc_str_;
  ss << " conf=" << std::dec << confidence_;
  ss << " stride=" << std::hex;
  if (stride_ < 0) {
    ss << "-0x" << (-stride_);
  } else {
    ss << "0x" << stride_;
  }
  ss << " val=0x" << last_value_;
  ss << " ptr=" << std::dec << predict_ptr_;
  return ss.str();
}

PredictionResult SingleStrideEntry::lookup(InflightEntry &op, uint32_t num_inflights) {
  PredictionResult result;
  // move pointer forward
  //++ predict_ptr_;
  predict_ptr_ = num_inflights;
  if (confidence_ >= conf_thd_) {
    op.single_stride_conf_hit = true;
    result.speculate = true;
    result.predicted_value = last_value_ + stride_ * predict_ptr_;
  } else {
    result.speculate = false;
  }
  op.single_stride_hit = true;
  if (logging_enable_) {
    std::cout << getDisplayStr() << " pred=0x" << std::hex;
    if (result.speculate) {
      std::cout << result.predicted_value;
    } else {
      std::cout << "??";
    }
    std::cout << std::endl;
  }
  return result;
}

void SingleStrideEntry::specUpdate(InflightEntry& op) {
  if (op.prediction_result == 0) {
    // mis-prediction
    assert(!op.single_stride_conf_hit);
    analysis::Utils::decrConf(confidence_, conf_min_, 1);
  } else  if (op.prediction_result == 1) {
    // correct prediction
    assert(op.single_stride_conf_hit);
    analysis::Utils::incrConf(confidence_, conf_max_, 1);
  }
}

void SingleStrideEntry::finalUpdate(const InflightEntry& op) {
  // reference point move forward; so pointer backward
  //-- predict_ptr_;
  assert(confidence_ > 0);
  assert(last_value_ != 0xdeadbeef);
  // training
  const int64_t curr_stride = static_cast<int64_t>(op.value - last_value_);
  if (confidence_ == 1) {
    confidence_ = conf_min_;
    stride_ = curr_stride;
  } else {
    assert(stride_ != 0xdeadbeef);
    if (stride_ == curr_stride) {
      // stride repeating, enable it
      confidence_ = conf_thd_;
    } else {
      confidence_ = conf_min_;
      stride_ = curr_stride;
    }
  }
}

SingleStrideEntry::SingleStrideEntry(
  InflightEntry&op, const std::string& pc_to_watch)
{
  confidence_ = 1;
  tag_ = calcTag(op.pc, op.piece);
  last_value_ = op.value;
  stride_ = 0xdeadbeef;
  predict_ptr_ = 0;
  // debug
  pc_str_ = analysis::Utils::getPcStr(op.pc, op.piece);
  if (pc_str_ == pc_to_watch) {
    logging_enable_ = true;
  }
}

SingleStridePredictor::SingleStridePredictor(
  const BaseConfigs& config,
  uint64_t pc,
  uint32_t piece) :
  BasePredictor(pc, piece),
  num_sets_ (config.ss_num_sets),
  num_ways_ (config.ss_num_ways),
  entries_ (num_sets_, std::vector<SingleStrideEntry::Handle>(num_ways_, nullptr)),
  rr_ptr_ (num_sets_, 0)
{ }

uint32_t SingleStridePredictor::calcIndex(const InflightEntry& op) const {
  const uint64_t ex_pc = op.pc + static_cast<uint64_t>(op.piece);
  const uint64_t index = ex_pc & (((uint64_t)0x1 << num_sets_) - 1);
  return static_cast<uint32_t>(index);
}

SingleStrideEntry::Handle SingleStridePredictor::findEntry(const InflightEntry& op) {
  for (auto& entry : entries_[calcIndex(op)]) {
    if (entry->isTagMatch(op)) {
      return entry;
    }
  }
  return nullptr;
}

PredictionResult SingleStridePredictor::lookup(InflightEntry& op, uint32_t num_inflights) {
  auto entry = findEntry(op);
  if (entry) {
    return entry->lookup(op, num_inflights);
  } else {
    PredictionResult result;
    result.speculate = false;
    return result;
  }
}

void SingleStridePredictor::specUpdate(InflightEntry& op) {
  if (op.single_stride_hit) {
    if (op.single_stride_conf_hit) {
      if (op.prediction_result == 0) {
        // incorrect; prepare for multi-stride training
        op.single_stride_mis_pred = true;
      }
    }
    auto entry = findEntry(op);
    assert(entry);
    entry->specUpdate(op);
  }
}

void SingleStridePredictor::finalUpdate(const InflightEntry& op) {
  auto entry = findEntry(op);
  if (entry) {
    entry->finalUpdate(op);
  } else {
    // allocate?
  }
}


FuzzyMultiStrideEntry::FuzzyMultiStrideEntry() :
  stride_count_ (2 * max_multi_stride_)
{ }

FuzzyMultiStridePredictor::FuzzyMultiStridePredictor(const BaseConfigs& config) :
  num_sets_ (config.fms_num_sets),
  num_ways_ (config.fms_num_ways),
  entries_ (num_sets_, std::vector<FuzzyMultiStrideEntry::Handle>(num_ways_, nullptr)),
  rr_ptr_ (num_sets_, 0)
{ }

}

// ---------------- ---------------- ----------------

PredictionResult getPrediction(const PredictionRequest& req) {
  inst_tracker_->addInstIssue(
      req.seq_no, req.pc, req.piece, req.is_candidate,
      static_cast<uint32_t>(req.cache_hit));
  PredictionResult res;
  res.predicted_value = 0x0;
  res.speculate = true;
  return res;
}

void speculativeUpdate(uint64_t seq_no,
    bool eligible,
    uint8_t prediction_result,
    uint64_t pc,
    uint64_t next_pc,
    InstClass insn,
    uint8_t piece,
    uint64_t src1,
    uint64_t src2,
    uint64_t src3,
    uint64_t dst) {
  inst_tracker_->addInstExec(
      seq_no, static_cast<uint32_t>(insn), src1, src2, src3, dst);
}

void updatePredictor(uint64_t seq_no,
    uint64_t actual_addr,
    uint64_t actual_value,
    uint64_t actual_latency) {
  inst_tracker_->addInstRetire(
      seq_no, actual_addr, actual_value, actual_latency);
}

void beginPredictor(int argc_other, char **argv_other) {
  if (argc_other > 0) {
    std::cout << "CONTESTANT ARGUMENTS:" << std::endl;
  }

  for (int i = 0; i < argc_other; i++) {
    std::cout << "\targv_other[" << i << "] = " << argv_other[i];
    std::cout << std::endl;
  }

  inst_tracker_.reset(new analysis::InstTracker(
    0xffff00000844f73c, 0
  ));
}

void endPredictor() {
  inst_tracker_->dump();
}
