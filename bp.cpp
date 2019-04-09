//
// Created by Bar on 08/04/2019.
//


#include "bp_api.h"
#include <vector>
#include <map>
#include <math.h>
#include <assert.h>

#define MAX_HISTORY_SIZE 64

typedef struct {
    bool branch;
    uint32_t target;
} prediction_t;
typedef uint32_t tag_t;
typedef uint64_t historyEntry_t;

typedef struct {
    uint32_t target;
    uint32_t idx;
} btbEntry_t;

enum SHARE_POLICY {
    NO_SHARE = 0,
    LSB_SHARE = 1,
    MID_SHARE = 2,
};

enum FSM_PRED {
    STRONG_NOT_TAKEN = 0,
    WEAK_NOT_TAKEN = 1,
    WEAK_TAKEN = 2,
    STRONG_TAKEN = 3,
};

class BranchP {
    SIM_stats m_stats;
    unsigned m_btbSize;
    unsigned m_tagSize;
    FSM_PRED m_fsmState;
    bool m_isGlobalHist;
    bool m_isGlobalTable;
    SHARE_POLICY m_shared;
    std::map<tag_t, btbEntry_t> m_btb;
    std::vector<historyEntry_t> m_history;
    historyEntry_t m_history_mask;
    std::vector<std::vector<FSM_PRED>> m_FSM;

    uint32_t FindBTBVictim() {}


public:
    BranchP() = default; //added so the global parameter will be initialized
    BranchP(unsigned btbSize, unsigned historySize, unsigned tagSize,
            unsigned fsmState,
            bool isGlobalHist, bool isGlobalTable, int Shared) :
            m_btbSize(btbSize),
            m_history_mask(std::pow(2, historySize) - 1),
            m_tagSize(tagSize),
            m_fsmState(static_cast<FSM_PRED>(fsmState)),
            m_isGlobalHist(isGlobalHist),
            m_isGlobalTable(isGlobalTable),
            m_shared(static_cast<SHARE_POLICY>(Shared)) {
        assert(historySize <= MAX_HISTORY_SIZE);
        m_history = (m_isGlobalHist) ? std::vector<historyEntry_t>(1, 0)
                                     : std::vector<historyEntry_t>(m_btbSize,
                                                                   0);
        uint32_t N_fsm = (m_isGlobalTable) ? 1 : m_history.size();
        m_FSM = std::vector<std::vector<FSM_PRED>>(N_fsm, std::vector<FSM_PRED>(
                pow(2, historySize), m_fsmState));
    }

    void Update() {}

    prediction_t Predict(uint32_t pc);

    SIM_stats &GetStats() {}


};

prediction_t BranchP::Predict(uint32_t pc) {

    prediction_t resPred;
    auto tagMask = uint32_t(
            pow(2, m_tagSize + 1) - 1); //tag starts from the third bit
    tag_t branchTag = pc & tagMask;

    if (m_btb.count(branchTag)) {
        resPred.target = m_btb[branchTag].target;

        FSM_PRED fsmPred = m_FSM[m_btb[branchTag].idx]
                [m_history[m_btb[branchTag].idx]];

        (fsmPred == STRONG_NOT_TAKEN || fsmPred == WEAK_NOT_TAKEN)
        ? (resPred.branch = false) : (resPred.branch = true);

    }
    else {
        resPred = {.branch = false, .target = pc + 4};
    }

    return resPred;
}


BranchP globalBranchP;

/***********************************************************/
/* HW functions */
/***********************************************************/

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize,
            unsigned fsmState,
            bool isGlobalHist, bool isGlobalTable, int Shared) {

}


bool BP_predict(uint32_t pc, uint32_t *dst) {
    prediction_t result = globalBranchP.Predict(pc);
    assert(dst);
    *dst = result.target;
    return result.branch;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst) {

}

void BP_GetStats(SIM_stats *curStats) {

}