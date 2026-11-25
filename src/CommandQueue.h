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

#ifndef CMDQUEUE_H
#define CMDQUEUE_H

#include <vector>

#include "BankState.h"
#include "BusPacket.h"
#include "SimulatorObject.h"
#include "SystemConfiguration.h"
#include "Transaction.h"

using namespace std;

namespace DRAMSim
{
class Rank;
class CommandQueue : public SimulatorObject
{
    CommandQueue();
    ostream& dramsimLog;

  public:
               
    typedef vector<BusPacket*> BusPacket1D;
    typedef vector<BusPacket1D> BusPacket2D;
    typedef vector<BusPacket2D> BusPacket3D;

                
    CommandQueue(vector<vector<BankState>>& states, ostream& dramsimLog);
    virtual ~CommandQueue();

    void enqueue(BusPacket* newBusPacket);
    bool pop(BusPacket** busPacket);

                           
    bool process_refresh(BusPacket** busPacket);
    bool process_command(BusPacket** busPacket);
    bool process_precharge(BusPacket** busPacket);

    bool hasRoomFor(unsigned numberToEnqueue, unsigned rank, unsigned bank);
    bool isIssuable(BusPacket* busPacket);
                                                                                      
    struct WaitFlags {
        uint64_t barrier = 0, address_dependency = 0, read_write_timing = 0,
                 needs_activate = 0, wrong_open_row = 0, row_limit = 0,
                 activate_timing = 0, pim_bank_restricted = 0,
                 precharge_timing = 0, other = 0;
    };
                                                                              
                                                                                 
    struct CounterfactualFlags {
        uint64_t barrier_present = 0;
        uint64_t barrier_blocks_ready_command = 0;
        uint64_t dependency_blocks_ready_command = 0;
        uint64_t timing_only_blocked = 0;
        uint64_t issuable_but_not_selected = 0;
        uint64_t ready_behind_barrier_with_dependency = 0;
        uint64_t any_dram_ready_queued_command = 0;
        uint64_t head_dram_ready = 0;
    };
                                                                              
                                                                       
    struct TimingFlags {
        uint64_t wait_tRCD = 0, wait_tCCD_or_burst = 0, wait_bus_turnaround = 0;
        uint64_t wait_rank_turnaround = 0, wait_tRP = 0, wait_tRAS = 0;
        uint64_t wait_tRRD = 0, wait_tRC = 0, wait_tRFC = 0;
        uint64_t wait_tFAW = 0, wait_read_to_pre = 0, wait_write_to_pre = 0;
        uint64_t wait_row_not_open = 0, wait_wrong_row = 0, wait_bank_state = 0;
        uint64_t wait_pim_bank_restricted = 0, wait_row_access_limit = 0;
        uint64_t wait_unknown_gate = 0;
    };
                                                            
                                                                             
                                                                       
    struct SelectionOpportunity {
        uint64_t selected_column = 0;
        uint64_t selected_with_alternative = 0;
        uint64_t selected_with_other_bank = 0;
        uint64_t selected_with_same_bank = 0;
        uint64_t selected_with_better_lookahead = 0;
        uint64_t selected_with_equal_lookahead = 0;
        uint64_t selected_with_worse_lookahead = 0;
        uint64_t selected_pim_mode = 0;
        uint64_t pim_with_alternative = 0;
        uint64_t pim_with_other_bank = 0;
        uint64_t pim_with_better_lookahead = 0;
        uint64_t candidate_alternatives_total = 0;
        uint64_t candidate_other_bank_total = 0;
    };
    SelectionOpportunity getSelectionOpportunity() const { return selection_opportunity_; }
    TimingFlags diagnoseTimingNoIssue() const;
    WaitFlags diagnoseNoIssue() const;
    CounterfactualFlags diagnoseCounterfactualNoIssue() const;

    bool isEmpty(unsigned rank);
    void needRefresh(unsigned rank);

    void print();
    void update();                                
    vector<BusPacket*>& getCommandQueue(unsigned rank, unsigned bank);

             
    BusPacket3D queues;                                   
    vector<vector<BankState>>& bankStates;
    vector<Rank*>* ranks;

  private:
    void recordSelectionOpportunity(const BusPacket* selected);
    unsigned rowLookahead(unsigned rank, size_t start, const BusPacket* candidate) const;
    SelectionOpportunity selection_opportunity_;
    void nextRankAndBank(unsigned& rank, unsigned& bank);
             

    unsigned nextBank;
    unsigned nextRank;

    unsigned nextBankPRE;
    unsigned nextRankPRE;

    unsigned refreshRank;
    unsigned refreshBank;

    bool refreshWaiting;

    vector<vector<unsigned>> tXAWCountdown;
    vector<vector<unsigned>> rowAccessCounters;

    bool sendAct;

                                                
    unsigned num_ranks_;
    unsigned num_banks_;
    unsigned cmd_queue_depth_;
    unsigned xaw_;
    unsigned total_row_accesses_;
    SchedulingPolicy schedulingPolicy_;
    QueuingStructure queuingStructure_;
};

}                      
#endif
