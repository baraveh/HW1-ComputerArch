/* 046267 Computer Architecture - Spring 2019 - HW #1 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <cmath>
#include <vector>
#define TARGET_SIZE 30
#include <deque>
#include <iostream>
#include <bitset>

// History father class
class History {
public:
    virtual void update_history(bool next_br_action, uint32_t btb_index) = 0;
    virtual uint32_t get_index(uint32_t btb_index) = 0;
    virtual ~History() = default;
    virtual void reset(uint32_t btb_index) = 0;
};

class GlobalHist:public History {
public:
    GlobalHist(uint32_t historySize) : history(0), mask(0){
        for (uint32_t i = 0; i < historySize; i++)
            mask = (mask << 1u) + 1u;
    }

    // members
    uint32_t history;
    uint32_t mask;
    void update_history(bool next_br_action, uint32_t btb_index) override {
        history <<= 1u;
        history += uint32_t(next_br_action);
        history &= mask;
    }
    uint32_t get_index( uint32_t btb_index) override {
        return history;
    }
    void reset(uint32_t btb_index) {
        return; // reset of global history has no effect.
    }
};

class LocalHist:public History {
public:
    uint32_t historySize;
    std::vector<uint32_t> history;
    uint32_t mask;
    LocalHist(uint32_t btbSize, uint32_t historySize) :
            historySize(historySize),
            history(std::vector<uint32_t>(btbSize, 0)),
            mask(0) {
        for (uint32_t i = 0; i < historySize; i++)
            mask = (mask << 1u) + 1u;
    }
    void update_history(bool next_br_action, uint32_t btb_index) override {
        history.at(btb_index) <<= 1u;
        history.at(btb_index) += uint32_t(next_br_action);
        history.at(btb_index) &= mask;
    }
    uint32_t get_index(uint32_t btb_index) override {
        return history.at(btb_index);
    }
    void reset(uint32_t btb_index) {
        history.at(btb_index) = 0;
    }
};


enum FSM {SNT = 0, WNT = 1, WT = 2, ST = 3};
// FSM table father class
class FSMTable {
public:
    virtual void update_state(bool taken_or_not, uint32_t btb_index, uint32_t fsm_table_index) = 0;
    virtual bool get_prediction(uint32_t btb_index, uint32_t fsm_table_index) = 0;
	virtual int get_pred_debug(uint32_t btb_index, uint32_t fsm_table_index) = 0;
    virtual ~FSMTable() = default;
    virtual void reset(uint32_t btb_index) = 0;
};

class GlobalTable:public FSMTable {
public:
    GlobalTable(uint32_t fsmTableSize, uint32_t fsmState) : fsmTableSize(fsmTableSize),
    global_table(std::vector<int>(fsmTableSize, fsmState)) {}
    uint32_t fsmTableSize;
    std::vector<int> global_table;
    void update_state(bool taken_or_not, uint32_t
                        btb_index, uint32_t fsm_table_index) override {
        if (taken_or_not) {
            if (global_table.at(fsm_table_index) == 3)
                return;
            global_table.at(fsm_table_index) += 1;
            return;
        }
        if (global_table.at(fsm_table_index) == 0)
            return;
        global_table.at(fsm_table_index) -= 1;
        return;
    }
	int get_pred_debug(uint32_t btb_index, uint32_t fsm_table_index) override {
		return global_table.at(fsm_table_index);
	}
    bool get_prediction( uint32_t btb_index, uint32_t fsm_table_index) override {
        return global_table.at(fsm_table_index) > 1; // if it's bigger than
        // one, the prediction is taken.
    }
    //reset in global has not effect.
    void reset(uint32_t btb_index) override {
        return;
    }
};

class LocalTable: public FSMTable {
public:
    uint32_t fsmTableSize;
    uint32_t btbSize;
    uint32_t fsmState;
    std::vector<std::vector<int>> local_table;
    LocalTable(uint32_t btbSize, uint32_t fsmTableSize, uint32_t fsmState) :
            fsmTableSize(fsmTableSize),
            btbSize(btbSize),
            fsmState(fsmState),
            local_table(std::vector<std::vector<int>>(btbSize, std::vector<int>
                    (fsmTableSize, fsmState))) {}

    void update_state(bool taken_or_not, uint32_t btb_index, uint32_t fsm_table_index) override {
        if (taken_or_not) {
            if (local_table.at(btb_index).at(fsm_table_index) == 3)
                return;
            local_table.at(btb_index).at(fsm_table_index) += 1;
            return;
        }
        if (local_table.at(btb_index).at(fsm_table_index) == 0)
            return;
        local_table.at(btb_index).at(fsm_table_index) -= 1;
        return;
    }
	int get_pred_debug(uint32_t btb_index, uint32_t fsm_table_index) override {
		return local_table.at(btb_index).at(fsm_table_index);
	}
    bool get_prediction(uint32_t btb_index, uint32_t fsm_table_index) override {
        return local_table.at(btb_index).at(fsm_table_index) > 1;
    }
    void reset(uint32_t btb_index) override {
        local_table.at(btb_index) = std::vector<int>(fsmTableSize, fsmState);
        return;
    }
};



enum shareMode {none = 0, lsb_share = 1, mid_share = 2};
class BTB {
public:
    std::vector<uint32_t> destinations;
    std::vector<int64_t> tags;
    History* history;
    FSMTable* fsm_table;
    int share_mode;
    uint32_t tagSize;
    uint32_t btb_mask;
    uint32_t tag_mask;
    uint32_t historySize;
    SIM_stats sim_stats;
    uint32_t fsmState;
    BTB(uint32_t btbSize, uint32_t historySize, uint32_t tagSize, uint32_t fsmState,
        bool isGlobalHist, bool isGlobalTable, int Shared) : destinations
        (btbSize, 0), tags(btbSize, -1), history(nullptr),
                                                                fsm_table(nullptr), share_mode(Shared), tagSize(tagSize),
                                                                historySize(historySize), fsmState(fsmState) {
        sim_stats.size = 0;
        sim_stats.size += btbSize * (tagSize + TARGET_SIZE);
        if (isGlobalHist) {
            try {
                history = new GlobalHist(historySize);
            } catch(...) {
                throw;
            }
            sim_stats.size += historySize;
        }
        else {
            try {
                history = new LocalHist(btbSize, historySize);
            } catch(...) {
                throw;
            }
            sim_stats.size += btbSize * historySize;
        }

        if (isGlobalTable) {
            try {
                fsm_table = new GlobalTable(uint32_t(pow(2, historySize)),
                                            fsmState);
            } catch(...) {
                delete history;
                throw;
            }
            sim_stats.size += unsigned(pow(2, historySize)) * 2;
        }
        else {
            try {
                fsm_table = new LocalTable(btbSize,
                                           uint32_t(pow(2, historySize)),
                                           fsmState);
            } catch (...) {
                delete history;
                throw;
            }
            sim_stats.size += btbSize * unsigned(pow(2, historySize)) * 2;
        }

        btb_mask = 0;
        for (uint32_t i = 0; i < log2(btbSize); i++)
            btb_mask = (btb_mask << 1u) + 1u;

        tag_mask = 0;
        for (uint32_t i = 0; i < tagSize; i++)
            tag_mask = (tag_mask << 1u) + 1u;
    }

    ~BTB() {
        delete history;
        delete fsm_table;
    }
    inline uint32_t get_btb_index(uint32_t pc) { // given pc, we return the
        // btb table index according to this pc. btb_mask is a mask according
        // to the size of the btb table.
        return (pc >> 2u) & btb_mask;
    }
    inline uint32_t get_tag(uint32_t pc) { // given pc, we return the
        // tag of the pc, according to the tag size.
        return (pc >> 2u) & tag_mask;
    }
    uint32_t get_fsm_table_index (uint32_t pc) { // given pc, we get it's
        // history based on it's btb table index (for global history it
        // doesn't matter). The history is the index of the fsm table, in
        // unsigned integer (of course we take into account the sharing
        // possibility.
        uint32_t history_val = history->get_index(get_btb_index(pc));
        if (share_mode == shareMode::none) {
            return history_val;
        }

        uint32_t history_mask = 0;
        for (uint32_t i = 0; i < historySize; i++) history_mask = (history_mask
                << 1u) + 1u;
        if (share_mode == shareMode::mid_share) pc >>= 14u;

        return ((pc >> 2u) & history_mask) ^ history_val;
    }
    bool predict(uint32_t pc, uint32_t *dst) {
        uint32_t tag = get_tag(pc);
        uint32_t btb_index = get_btb_index(pc);
        uint32_t fsm_table_index = get_fsm_table_index(pc);
		std::cout << "btb_index: " << btb_index << " tag: " << tag << " fsm_table_index: " << fsm_table_index << std::endl;
		std::cout << "hist: " << history->get_index(btb_index) << " pred: " << fsm_table->get_pred_debug(btb_index, fsm_table_index)<< std::endl;
        bool prediction = fsm_table->get_prediction(btb_index, fsm_table_index);
        if (!prediction || tags.at(btb_index) != int64_t(tag)) { // if we
            // predicted not taken or if the tag of the received pc doesn't
            // match the tag that is already inside the btb table.
            *dst = pc + 4;
            return false;
        }
        else {
            *dst = destinations.at(btb_index);
            return true;
        }
    }

    void update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
        sim_stats.br_num++;
        if ((pred_dst != pc + 4 && !taken) || (pred_dst != targetPc && taken)) {
            sim_stats.flush_num++;
        }
        uint32_t tag = get_tag(pc);
        uint32_t btb_index = get_btb_index(pc);
        if(tag != tags.at(btb_index)) {
            fsm_table->reset(btb_index);
            history->reset(btb_index);
            tags.at(btb_index) = tag;
        }
        fsm_table->update_state(taken, btb_index, get_fsm_table_index(pc));
        history->update_history(taken, btb_index);
        destinations.at(btb_index) = targetPc;
    }

    void GetStats(SIM_stats *curStats) {
        *curStats = sim_stats;
    }
};

static BTB* btb;
int BP_init(uint32_t btbSize, uint32_t historySize, uint32_t tagSize, uint32_t fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
    try {
        btb = new BTB(btbSize, historySize, tagSize, fsmState, isGlobalHist,
                      isGlobalTable, Shared);
    } catch(...) {
        throw;
    }
	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
    return btb->predict(pc, dst);
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	btb->update(pc, targetPc, taken, pred_dst);
}

void BP_GetStats(SIM_stats *curStats){
    btb->GetStats(curStats);
    delete btb;
}

