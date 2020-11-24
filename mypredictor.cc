#include <stdio.h>
#include <inttypes.h>
#include "cvp.h"
#include "mypredictor.h"


PredictionResult getPrediction(const PredictionRequest& req) {
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
}

void updatePredictor(uint64_t seq_no,
        uint64_t actual_addr,
        uint64_t actual_value,
        uint64_t actual_latency) {
}

void beginPredictor(int argc_other, char **argv_other) {
    if (argc_other > 0)
        printf("CONTESTANT ARGUMENTS:\n");

    for (int i = 0; i < argc_other; i++)
        printf("\targv_other[%d] = %s\n", i, argv_other[i]);
}

void endPredictor() {
}