//
// Created by Bar on 08/04/2019.
//


#include "bp_api.h"
#include <vector>
#include <map>
#include <math.h>
#include <assert.h>

#define MAX_HISTORY_SIZE 8
#define POW2(exp) (1 << (exp))

typedef struct {
    bool branch;
    uint32_t target;
} prediction_t;
typedef uint32_t tag_t;
typedef uint32_t historyEntry_t;

typedef struct {
    uint32_t tag;
    uint32_t target;
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
    std::vector<btbEntry_t> m_btb;
    std::vector<historyEntry_t> m_history;
    historyEntry_t m_historyMask;
    std::vector<std::vector<FSM_PRED>> m_FSM;
	uint32_t barnchCounter;
	uint32_t flushCounter;
	uint32_t m_idxMask;
	uint32_t m_tagMask;
    uint32_t FindBTBVictim() {
		
	}

	static uint32_t MSBIndex(uint32_t x) {
		uint32_t msb = 0;
		while (x) { // while there are still bits
			x >>= 1; // right-shift the argument
			msb++; // each time we right shift the argument, increment msb
		}
		return msb;
	}

	static uint32_t NBits(uint32_t x) { //number of positive bits in x
		uint32_t n = 0;
		while (x) {
			n += x & 1; //lsb
			x >>= 1;
		}
		return n;
	}

	static void UpdatePred(FSM_PRED& pred, bool update) {
		switch (pred)
		{
		case STRONG_NOT_TAKEN:
			pred = (update) ? WEAK_NOT_TAKEN : STRONG_NOT_TAKEN;
			break;
		case WEAK_NOT_TAKEN:
			pred = (update) ? WEAK_TAKEN : STRONG_NOT_TAKEN;
			break;
		case WEAK_TAKEN:
			pred = (update) ? STRONG_TAKEN : WEAK_NOT_TAKEN;
			break;
		case STRONG_TAKEN:
			pred = (update) ? STRONG_TAKEN : WEAK_TAKEN;
			break;
		default:
			break;
		}
	}

	uint32_t GetFSMIdx(uint32_t pc, uint32_t history) {
		history &= m_historyMask;
		pc >>= 2;
		uint32_t idx = history;
		switch (m_shared)
		{
		case NO_SHARE:
			return history;
		case LSB_SHARE:
			return history ^ pc;
		case MID_SHARE:
			pc >> 14;
			return history ^ pc;
		default:
			return history;
		}
	}

	inline tag_t GetTag(uint32_t pc) {
		return (pc & m_tagMask) >> 2;
	}

	inline uint32_t GetBTBIdx(uint32_t pc) {
		return (pc & m_idxMask) >> 2;
	}


public:
    BranchP() = default; //added so the global parameter will be initialized
    BranchP(unsigned btbSize, unsigned historySize, unsigned tagSize,
            unsigned fsmState,
            bool isGlobalHist, bool isGlobalTable, int Shared) :
            m_btbSize(btbSize),
            m_historyMask(std::pow(2, historySize) - 1),
            m_tagSize(tagSize),
            m_fsmState(static_cast<FSM_PRED>(fsmState)),
            m_isGlobalHist(isGlobalHist),
            m_isGlobalTable(isGlobalTable),
            m_shared(static_cast<SHARE_POLICY>(Shared)),
			barnchCounter(0),
			flushCounter(0) {

        assert(historySize <= MAX_HISTORY_SIZE);
        m_history = (m_isGlobalHist) ? std::vector<historyEntry_t>(1, 0)
                                     : std::vector<historyEntry_t>(m_btbSize, 0);
        uint32_t N_fsm = (m_isGlobalTable) ? 1 : m_history.size();
        m_FSM = std::vector<std::vector<FSM_PRED>>(N_fsm, std::vector<FSM_PRED>(
			POW2(historySize), m_fsmState));

		uint32_t bitsRequired = (NBits(m_btbSize) == 1) ? MSBIndex(m_btbSize) - 1 :
			MSBIndex(m_btbSize); //number of bits required to index btb
		m_idxMask = (POW2(bitsRequired) - 1) << 2;

		m_tagMask = (POW2(tagSize) - 1) << 2;
		
    }

    void Update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst) {
		tag_t tag = GetTag(pc);
		uint32_t btbIdx = GetBTBIdx(pc);
		uint32_t fsmTableIdx = (m_isGlobalTable) ? 0 : btbIdx;
		uint32_t histIdx = (m_isGlobalHist) ? 0 : btbIdx;
		historyEntry_t& history = m_history[histIdx];
		FSM_PRED& prevPred = m_FSM[fsmTableIdx][GetFSMIdx(pc, history)];
		UpdatePred(prevPred, taken);
		//update history
		history << 1;
		history += static_cast<historyEntry_t>(taken);
		history & m_historyMask;
	}

	prediction_t Predict(uint32_t pc) {
		prediction_t resPred;
		tag_t tag = GetTag(pc);
		uint32_t btbIdx = GetBTBIdx(pc);
		uint32_t fsmTableIdx = (m_isGlobalTable) ? 0 : btbIdx;
		uint32_t histIdx = (m_isGlobalHist) ? 0 : btbIdx;

		prediction_t resPred = { false, pc + 4 };
		if (m_btb[btbIdx].tag != tag) { // cant find tag
			return;
		}
		resPred.target = m_btb[btbIdx].target;

		historyEntry_t hist = m_history[histIdx];
		FSM_PRED pred = m_FSM[fsmTableIdx][GetFSMIdx(pc, hist)];
		if (pred == STRONG_NOT_TAKEN || pred == WEAK_NOT_TAKEN) {
			resPred.branch = false;
		}
		else {
			resPred.branch = true;
		}
		return resPred;
	}

    SIM_stats &GetStats() {}


};



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