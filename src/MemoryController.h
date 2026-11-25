/*********************************************************************************
 *  Copyright (c) 2010-2011, Elliott Cooper-Balis
 *                             Paul Rosenfeld
 *                             Bruce Jacob
 *                             University of Maryland
 *                             dramninjas [at] gmail [dot] com
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright notice,
 *        this list of conditions and the following disclaimer in the documentation
 *        and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************************/

#ifndef MEMORYCONTROLLER_H
#define MEMORYCONTROLLER_H

#include <map>
#include <vector>

#include "BankState.h"
#include "BusPacket.h"
#include "CSVWriter.h"
#include "CommandQueue.h"
#include "Configuration.h"
#include "Rank.h"
#include "SimulatorObject.h"
#include "SystemConfiguration.h"
#include "Transaction.h"

using namespace std;

namespace DRAMSim
{
class Rank;
class MemorySystem;
class MemoryControllerStats;
class MemoryController : public SimulatorObject
{
  public:
                
    MemoryController(MemorySystem* ms, CSVWriter& csvOut_, ostream& simLog, Configuration& config);
    virtual ~MemoryController();

    bool addTransaction(Transaction* trans);
    void returnReadData(const Transaction* trans);
    void receiveFromBus(BusPacket* bpacket);
    void attachRanks(vector<Rank*>* ranks);
    void update();
    void printDebugOnUpate();
    void printStats(bool finalStats = false);
                                                                                  
                                                                                   
                                                                           
    struct RowBankProfile {
        uint64_t enqueued_column_requests = 0;
        uint64_t enqueue_row_open_hit = 0, enqueue_wrong_open_row = 0;
        uint64_t enqueue_row_not_open = 0;
        uint64_t column_reads = 0, column_writes = 0;
        uint64_t pim_column_reads = 0, pim_column_writes = 0;
        uint64_t activate = 0, precharge = 0;
        uint64_t activate_same_row = 0, activate_different_row = 0;
        uint64_t row_hit_on_issue = 0, row_not_open_on_issue = 0;
        uint64_t wrong_row_on_issue = 0;
        uint64_t pim_row_hit_on_issue = 0;
        uint64_t active_bank_samples = 0;
        uint64_t active_bank_sample_sum = 0;
        uint64_t active_bank_sample_max = 0;
    };
    struct BaselineCommandCounters {
        uint64_t cycles, queued_no_issue, read, write, activate, precharge, refresh,
                 pim_read, pim_write;
        CommandQueue::WaitFlags waits;
        CommandQueue::CounterfactualFlags counterfactual;
        CommandQueue::TimingFlags timing;
        RowBankProfile row_bank;
        CommandQueue::SelectionOpportunity selection_opportunity;
    };
                                                             
                                                                               
                                                                   
    struct Step13RestoreStats {
        uint64_t restore_issued = 0;
        uint64_t cooldown_cycles = 0;
        uint64_t cooldown_with_queued_commands = 0;
        unsigned extra_cycles = 0;
    };
    Step13RestoreStats getStep13RestoreStats() const {
        return {step13_restore_issued_, step13_cooldown_cycles_,
                step13_cooldown_with_queue_, step13_extra_cycles_};
    }
    BaselineCommandCounters getBaselineCounters() const;
    const std::vector<RowBankProfile>& getPerBankRowProfiles() const { return step3_per_bank; }
                                                                       
    struct CommandStreamProfile {
        uint64_t issued = 0, tagged = 0, barrier_tagged = 0;
        uint64_t mac_tagged = 0, mac_pim_mode = 0, crf_tagged = 0;
        uint64_t grf_tagged = 0, mode_tagged = 0, other_tagged = 0;
        uint64_t consecutive_same_bank_columns = 0, consecutive_other_bank_columns = 0;
        uint64_t consecutive_same_rank_columns = 0, column_streams = 0;
        uint64_t mac_runs = 0, max_mac_run = 0;
        uint64_t barrier_tagged_columns = 0, pim_mode_column_commands = 0;
    };
    CommandStreamProfile getCommandStreamProfile() const { return step5_stream_; }
                                                                      
                                                                            
    struct PIMPhaseProfile {
        uint64_t mac_runs_completed = 0, mac_run_length_sum = 0;
        uint64_t mac_gap_events = 0, mac_gap_commands = 0, mac_gap_cycles = 0;
        uint64_t max_mac_gap_commands = 0, max_mac_gap_cycles = 0;
        uint64_t mac_break_grf = 0, mac_break_barrier = 0;
        uint64_t mac_break_crf = 0, mac_break_mode = 0;
        uint64_t mac_break_actpre = 0, mac_break_other = 0;
        uint64_t gap_grf = 0, gap_barrier = 0, gap_crf = 0;
        uint64_t gap_mode = 0, gap_actpre = 0, gap_other = 0;
        uint64_t grf_before_mac_run = 0, barrier_before_mac_run = 0;
        uint64_t mac_run_bank_switches = 0;
        uint64_t phase_setup_commands = 0, phase_compute_commands = 0;
        uint64_t phase_teardown_commands = 0;
        uint64_t phase_first_to_last_mac_cycles = 0;
        uint64_t phase_first_to_last_mac_valid = 0;
        uint64_t barrier_tagged_grf = 0, barrier_tagged_mac = 0;
        uint64_t barrier_tagged_other = 0;
        uint64_t grf_load_tagged = 0, grf_store_tagged = 0;
    };
    PIMPhaseProfile getPIMPhaseProfile() const;

    void resetStats();
    bool WillAcceptTransaction();
    bool addBarrier();

             
    vector<Transaction*> transactionQueue;

  private:
    ostream& dramsimLog;
    vector<vector<BankState>> bankStates;

                
    void insertHistogram(unsigned latencyValue, unsigned rank, unsigned bank);
    void updateCommandQueue(BusPacket* poppedBusPacket);
    void updateTransactionQueue();
    void updateBankState();
    void updateRefresh();
    void setBankStatesRW(size_t rank, size_t bank, uint64_t nextRead, uint64_t nextWrite,
                         TimingOrigin readOrigin, TimingOrigin writeOrigin);
    void setBankStates(size_t rank, size_t bank, CurrentBankState currentBankState,
                       BusPacketType lastCommand, uint64_t stateChangeCountdown, uint64_t nextAct);

                                                   
    std::vector<RowBankProfile> step3_per_bank;
    std::vector<unsigned> step3_last_activated_row;
    std::vector<bool> step3_has_last_activated_row;
    RowBankProfile step3_aggregate;
    void recordStep3Command(const BusPacket* packet);

    CommandStreamProfile step5_stream_;
    bool step5_prev_column_ = false, step5_prev_mac_ = false;
    unsigned step5_prev_rank_ = 0, step5_prev_bank_ = 0;
    uint64_t step5_current_mac_run_ = 0;
    uint64_t step5_trace_count_ = 0;
    void recordStep5IssuedCommand(const BusPacket* packet);
    PIMPhaseProfile step6_phase_;
    uint64_t step6_last_mac_cycle_ = 0, step6_first_mac_cycle_ = 0;
    uint64_t step6_last_mac_issued_index_ = 0, step6_first_mac_issued_index_ = 0;
    uint64_t step6_run_length_ = 0, step6_gap_commands_ = 0;
    uint64_t step6_gap_grf_ = 0, step6_gap_barrier_ = 0;
    uint64_t step6_trace_count_ = 0;
    bool step6_seen_mac_ = false, step6_in_run_ = false;
    bool step6_first_gap_command_ = false;
    bool step6_prev_mac_bank_valid_ = false;
    unsigned step6_prev_mac_rank_ = 0, step6_prev_mac_bank_ = 0;
    void recordStep6IssuedCommand(const BusPacket* packet);


             
    MemorySystem* parentMemorySystem;

    CommandQueue commandQueue;
    BusPacket* poppedBusPacket;
    vector<BusPacket*> writeDataToSend;
    vector<unsigned> writeDataCountdown;
    vector<Transaction*> returnTransaction;
    vector<Transaction*> pendingReadTransactions;
    map<unsigned, unsigned> latencies;                                 
    vector<bool> powerDown;
    vector<Rank*>* ranks;

                  
    CSVWriter& csvOut;

                                                                             
    BusPacket *outgoingCmdPacket, *outgoingDataPacket;
    unsigned cmdCyclesLeft, dataCyclesLeft;

    uint64_t totalTransactions, totalRefreshes;
    vector<uint64_t> grandTotalBankAccesses, totalReadsPerBank, totalWritesPerBank;
    vector<uint64_t> totalReadsPerRank, totalWritesPerRank;
    vector<uint64_t> totalActivatesPerBank, totalActivatesPerRank, totalEpochLatency;
    unsigned refreshRank, refreshBank;
    vector<unsigned> refreshCountdown, refreshCountdownBank;
    Configuration& config;
    MemoryControllerStats* memoryContStats;
      CommandQueue::WaitFlags baseline_waits;
      CommandQueue::CounterfactualFlags baseline_counterfactual;
      CommandQueue::TimingFlags baseline_timing;
      unsigned step13_extra_cycles_ = 0;
      unsigned step13_cooldown_remaining_ = 0;
      uint64_t step13_restore_issued_ = 0;
      uint64_t step13_cooldown_cycles_ = 0;
      uint64_t step13_cooldown_with_queue_ = 0;
      uint64_t baseline_cycles = 0;
      uint64_t baseline_queued_no_issue_cycles = 0;
      uint64_t baseline_issued_read_commands = 0;
      uint64_t baseline_issued_write_commands = 0;
      uint64_t baseline_issued_activate_commands = 0;
      uint64_t baseline_issued_precharge_commands = 0;
      uint64_t baseline_issued_refresh_commands = 0;
      uint64_t baseline_issued_pim_mode_read_commands = 0;
      uint64_t baseline_issued_pim_mode_write_commands = 0;


  public:
                                                                                  
    vector<uint64_t> backgroundEnergy, burstEnergy, actpreEnergy, refreshEnergy, aluPIMEnergy,
        readPIMEnergy;
    double totalBandwidth;

    uint64_t totalReads, totalWrites;
};

class MemoryControllerStats
{
  public:
    MemoryControllerStats(MemorySystem* parent, CSVWriter& csvOut_, ostream& simLog,
                          Configuration& configuration, uint64_t& totalTrans,
                          vector<uint64_t>& grandTotalBankAcc, vector<uint64_t>& totalReadsPerR,
                          vector<uint64_t>& totalWritesPerR, vector<uint64_t>& totalReadsPerB,
                          vector<uint64_t>& totalWritesPerB, vector<uint64_t>& totalActivatesPerR,
                          vector<uint64_t>& totalActivatesPerB, uint64_t& totalRef,
                          vector<uint64_t>& backgroundE, vector<uint64_t>& burstE,
                          vector<uint64_t>& actpreE, vector<uint64_t>& refreshE,
                          vector<uint64_t>& aluPIME, vector<uint64_t>& readPIME,
                          vector<Transaction*>& pendingReadTrans)
        : csvOut(csvOut_),
          dramsimLog(simLog),
          config(configuration),
          totalTransactions(totalTrans),
          grandTotalBankAccesses(grandTotalBankAcc),
          totalReadsPerRank(totalReadsPerR),
          totalWritesPerRank(totalWritesPerR),
          totalReadsPerBank(totalReadsPerB),
          totalWritesPerBank(totalWritesPerB),
          totalActivatesPerRank(totalActivatesPerR),
          totalActivatesPerBank(totalActivatesPerB),
          totalRefreshes(totalRef),
          backgroundEnergy(backgroundE),
          burstEnergy(burstE),
          actpreEnergy(actpreE),
          refreshEnergy(refreshE),
          aluPIMEnergy(aluPIME),
          readPIMEnergy(readPIME),
          pendingReadTransactions(pendingReadTrans)
    {
        parentMemorySystem = parent;
        totalEpochLatency = vector<uint64_t>(config.NUM_RANKS * config.NUM_BANKS, 0);
        resetStats();
    }

    void printStats(bool finalStats, unsigned myChannel, uint64_t currentClockCycle);
    void insertHistogram(unsigned latencyValue, unsigned rank, unsigned bank);
    void resetStats();

  private:
    MemorySystem* parentMemorySystem;
    ostream& dramsimLog;
    Configuration& config;
    CSVWriter& csvOut;

    uint64_t& totalTransactions;
    vector<uint64_t>& grandTotalBankAccesses;
    vector<uint64_t>& totalReadsPerRank;
    vector<uint64_t>& totalWritesPerRank;
    vector<uint64_t>& totalReadsPerBank;
    vector<uint64_t>& totalWritesPerBank;
    vector<uint64_t>& totalActivatesPerRank;
    vector<uint64_t>& totalActivatesPerBank;
    uint64_t& totalRefreshes;
    vector<uint64_t>& backgroundEnergy;
    vector<uint64_t>& burstEnergy;
    vector<uint64_t>& actpreEnergy;
    vector<uint64_t>& refreshEnergy;
    vector<uint64_t>& aluPIMEnergy;
    vector<uint64_t>& readPIMEnergy;
    vector<Transaction*>& pendingReadTransactions;

    uint64_t currentClockCycle;
    double totalBandwidth;
    map<unsigned, unsigned> latencies;
    vector<uint64_t> totalEpochLatency;

                                                                                         
  private:
                                                             
      uint64_t profile_total_cycles;
      uint64_t profile_trp_stall_cycles;
      uint64_t profile_trcd_stall_cycles;
      uint64_t profile_alu_active_cycles;
      uint64_t profile_total_stalls;

      void print_architectural_profile();


};

}                      

#endif

  

                                           
