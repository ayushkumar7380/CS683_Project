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

#include "CommandQueue.h"

#include <assert.h>

#include "AddressMapping.h"
#include "MemoryController.h"

using namespace DRAMSim;

CommandQueue::CommandQueue(vector<vector<BankState>>& states, ostream& simLog)
    : dramsimLog(simLog),
      bankStates(states),
      nextBank(0),
      nextRank(0),
      nextBankPRE(0),
      nextRankPRE(0),
      refreshRank(0),
      refreshBank(0),
      refreshWaiting(false),
      sendAct(true)
{
                                       
    currentClockCycle = 0;

                            
    num_ranks_ = getConfigParam(UINT, "NUM_RANKS");
    num_banks_ = getConfigParam(UINT, "NUM_BANKS");
    cmd_queue_depth_ = getConfigParam(UINT, "CMD_QUEUE_DEPTH");
    xaw_ = getConfigParam(UINT, "XAW");
    total_row_accesses_ = getConfigParam(UINT, "TOTAL_ROW_ACCESSES");
    schedulingPolicy_ = PIMConfiguration::getSchedulingPolicy();
    queuingStructure_ = PIMConfiguration::getQueueingStructure();

                                                       
    size_t numBankQueues;
    if (queuingStructure_ == PerRank)
    {
        numBankQueues = 1;
    }
    else if (queuingStructure_ == PerRankPerBank)
    {
        numBankQueues = num_banks_;
    }
    else
    {
        ERROR("== Error - Unknown queuing structure");
        exit(0);
    }

                                                                      
    rowAccessCounters = vector<vector<unsigned>>(num_ranks_, vector<unsigned>(num_banks_, 0));

                                                  
    BusPacket1D actualQueue;
    BusPacket2D perBankQueue = BusPacket2D();
    queues = BusPacket3D();
    for (size_t rank = 0; rank < num_ranks_; rank++)
    {
                                                                            
                            
        for (size_t bank = 0; bank < numBankQueues; bank++)
        {
            actualQueue = BusPacket1D();
            perBankQueue.push_back(actualQueue);
        }
        queues.push_back(perBankQueue);
    }

                               
                                                                         
                                
      
                                                                        
                                                 
    tXAWCountdown.reserve(num_ranks_);
    for (size_t i = 0; i < num_ranks_; i++)
    {
        tXAWCountdown.push_back(vector<unsigned>());
    }
}

CommandQueue::~CommandQueue()
{
                                         
    size_t bankMax = num_ranks_;
    if (queuingStructure_ == PerRank)
    {
        bankMax = 1;
    }
    for (size_t r = 0; r < num_ranks_; r++)
    {
        for (size_t b = 0; b < bankMax; b++)
        {
            for (size_t i = 0; i < queues[r][b].size(); i++)
            {
                delete (queues[r][b][i]);
            }
            queues[r][b].clear();
        }
    }
}

                                      
void CommandQueue::enqueue(BusPacket* newBusPacket)
{
    unsigned rank = newBusPacket->rank;
    unsigned bank = newBusPacket->bank;
    if (queuingStructure_ == PerRank)
    {
        queues[rank][0].push_back(newBusPacket);
        if (queues[rank][0].size() > cmd_queue_depth_)
        {
            ERROR("== Error - Enqueued more than allowed in command queue");
            ERROR(
                "                        Need to call .hasRoomFor(int numberToEnqueue, "
                "unsigned rank, unsigned bank) first");
            exit(0);
        }
    }
    else if (queuingStructure_ == PerRankPerBank)
    {
        queues[rank][bank].push_back(newBusPacket);
        if (queues[rank][bank].size() > cmd_queue_depth_)
        {
            ERROR("== Error - Enqueued more than allowed in command queue");
            ERROR(
                "                        Need to call .hasRoomFor(int numberToEnqueue, "
                "unsigned rank, unsigned bank) first");
            exit(0);
        }
    }
    else
    {
        ERROR("== Error - Unknown queuing structure");
        exit(0);
    }
}

bool CommandQueue::process_refresh(BusPacket** busPacket)
{
    if (refreshWaiting)
    {
        bool sendREF = true;
        for (size_t b = 0; b < num_banks_; b++)
        {
            if (bankStates[refreshRank][b].currentBankState == RowActive)
            {
                sendREF = false;
                *busPacket =
                    new BusPacket(PRECHARGE, 0, 0, bankStates[refreshRank][b].openRowAddress,
                                  refreshRank, b, nullptr, dramsimLog);
                if (isIssuable(*busPacket))
                {
                    return true;
                }
                else
                {
                    delete *busPacket;
                }
            }
        }
        if (sendREF)
        {
            *busPacket = new BusPacket(REF, 0, 0, 0, refreshRank, 0, nullptr, dramsimLog);
            if (isIssuable(*busPacket))
            {
                refreshWaiting = false;
                return true;
            }
            else
            {
                delete *busPacket;
            }
        }
    }
    return false;
}

bool CommandQueue::process_command(BusPacket** busPacket)
{
    unsigned startingRank = nextRank;
    unsigned startingBank = nextBank;
                         
                            
    do
    {
        vector<BusPacket*>& queue = getCommandQueue(nextRank, nextBank);
        for (size_t i = 0; i < queue.size(); i++)
        {
            BusPacket* packet = queue[i];

            if (isIssuable(packet))
            {
                if (i != 0 && queue[i]->tag.find("BAR", 0) != std::string::npos)
                {
                    break;
                }
                else
                {
                    bool depend = false;
                    for (size_t j = 0; j < i; j++)
                    {
                        if (queue[i]->bank == queue[j]->bank && queue[i]->row == queue[j]->row &&
                            queue[i]->column == queue[j]->column)
                        {
                            depend = true;
                            break;
                        }
                        if (queue[j]->tag.find("BAR", 0) != std::string::npos)
                        {
                            depend = true;
                            break;
                        }
                    }
                    if (!depend)
                    {
                        recordSelectionOpportunity(packet);
                        *busPacket = packet;
                        queue.erase(queue.begin() + i);
                        return true;
                    }
                }
            }
        }

        for (size_t i = 0; i < queue.size(); i++)
        {
            if (i != 0 && queue[i]->tag.find("BAR", 0) != std::string::npos)
            {
                break;
            }

            BusPacket* packet = queue[i];
            if (bankStates[packet->rank][packet->bank].currentBankState == Idle)
            {
                *busPacket =
                    new BusPacket(ACTIVATE, packet->physicalAddress, packet->column, packet->row,
                                  packet->rank, packet->bank, nullptr, dramsimLog, packet->tag);
                if (isIssuable(*busPacket))
                {
                    return true;
                }
                else
                {
                    delete *busPacket;
                }
            }
        }

        if (queuingStructure_ == PerRank)
            nextRank = (nextRank + 1) % num_ranks_;
        else
            nextRankAndBank(nextRank, nextBank);
    } while (!(startingRank == nextRank && startingBank == nextBank));

    return false;
}

                                                                    
                                                                    
                                                                         
                                                                             
                                                                     
unsigned CommandQueue::rowLookahead(unsigned rank, size_t start,
                                    const BusPacket* candidate) const
{
    const auto& q = queues[rank][0];
    unsigned score = 0;
    for (size_t k = start + 1; k < q.size(); ++k)
    {
        if (q[k]->tag.find("BAR", 0) != std::string::npos) break;
        if ((q[k]->busPacketType == READ || q[k]->busPacketType == WRITE) &&
            q[k]->bank == candidate->bank && q[k]->row == candidate->row)
            ++score;
    }
    return score;
}

void CommandQueue::recordSelectionOpportunity(const BusPacket* selected)
{
    if (selected->busPacketType != READ && selected->busPacketType != WRITE) return;
    ++selection_opportunity_.selected_column;
    const bool pim = (*ranks)[selected->rank]->mode_ == dramMode::HAB_PIM;
    if (pim) ++selection_opportunity_.selected_pim_mode;

                                                                            
                                                                  
    unsigned selected_score = 0;
    for (unsigned r = 0; r < num_ranks_; ++r)
    {
        const auto& q = queues[r][0];
        for (size_t i = 0; i < q.size(); ++i)
            if (q[i] == selected) selected_score = rowLookahead(r, i, selected);
    }

    bool alternative = false, other_bank = false, same_bank = false;
    bool better = false, equal = false, worse = false;
    for (unsigned r = 0; r < num_ranks_; ++r)
    {
        const auto& q = queues[r][0];
        for (size_t i = 0; i < q.size(); ++i)
        {
            const BusPacket* p = q[i];
                                                                           
                                                                           
                                                                   
            if (i != 0 && p->tag.find("BAR", 0) != std::string::npos) break;
            if (p->busPacketType != READ && p->busPacketType != WRITE) continue;
            if (p == selected || !isIssuable(const_cast<BusPacket*>(p))) continue;
            bool dependency = false;
            for (size_t j = 0; j < i; ++j)
            {
                const BusPacket* earlier = q[j];
                if ((p->bank == earlier->bank && p->row == earlier->row &&
                     p->column == earlier->column) ||
                    earlier->tag.find("BAR", 0) != std::string::npos)
                {
                    dependency = true;
                    break;
                }
            }
            if (dependency) continue;
            alternative = true;
            ++selection_opportunity_.candidate_alternatives_total;
            if (r != selected->rank || p->bank != selected->bank)
            {
                other_bank = true;
                ++selection_opportunity_.candidate_other_bank_total;
            }
            else same_bank = true;
            const unsigned score = rowLookahead(r, i, p);
            if (score > selected_score) better = true;
            else if (score == selected_score) equal = true;
            else worse = true;
        }
    }
    if (alternative) ++selection_opportunity_.selected_with_alternative;
    if (other_bank) ++selection_opportunity_.selected_with_other_bank;
    if (same_bank) ++selection_opportunity_.selected_with_same_bank;
    if (better) ++selection_opportunity_.selected_with_better_lookahead;
    if (equal) ++selection_opportunity_.selected_with_equal_lookahead;
    if (worse) ++selection_opportunity_.selected_with_worse_lookahead;
    if (pim)
    {
        if (alternative) ++selection_opportunity_.pim_with_alternative;
        if (other_bank) ++selection_opportunity_.pim_with_other_bank;
        if (better) ++selection_opportunity_.pim_with_better_lookahead;
    }
}

bool CommandQueue::process_precharge(BusPacket** busPacket)
{
    unsigned startingRank = nextRankPRE;
    unsigned startingBank = nextBankPRE;

    do
    {
        bool found = false;
        vector<BusPacket*>& queue = getCommandQueue(nextRankPRE, nextBankPRE);
        for (size_t i = 0; i < queue.size(); i++)
        {
            BusPacket* packet = queue[i];
            if (nextRankPRE == packet->rank && nextBankPRE == packet->bank &&
                bankStates[packet->rank][packet->bank].currentBankState == RowActive &&
                packet->row == bankStates[packet->rank][packet->bank].openRowAddress)
                found = true;
            if (packet->tag.find("BAR", 0) != std::string::npos)
                break;
        }
        if (!found)
        {
            *busPacket =
                new BusPacket(PRECHARGE, 0, 0, bankStates[nextRankPRE][nextBankPRE].openRowAddress,
                              nextRankPRE, nextBankPRE, nullptr, dramsimLog);
            if (isIssuable(*busPacket))
                return true;
            else
                delete *busPacket;
        }
        nextRankAndBank(nextRankPRE, nextBankPRE);
    } while (!(startingRank == nextRankPRE && startingBank == nextBankPRE));

    return false;
}

bool CommandQueue::pop(BusPacket** busPacket)
{
    if (queuingStructure_ == PerRankPerBank)
    {
        ERROR("== Error - queuingStructure_ PerRankPerBank is not allowed");
        exit(0);
    }
    for (size_t i = 0; i < num_ranks_; i++)
    {
                                                   
        for (size_t j = 0; j < tXAWCountdown[i].size(); j++) tXAWCountdown[i][j]--;
                                                                                     
        if (tXAWCountdown[i].size() > 0 && tXAWCountdown[i][0] == 0)
            tXAWCountdown[i].erase(tXAWCountdown[i].begin());
    }

    if (process_refresh(busPacket))
        return true;
    else if (process_command(busPacket))
        return true;
    else if (process_precharge(busPacket))
        return true;
    else
        return false;
    return false;
}

                                                                          
bool CommandQueue::hasRoomFor(unsigned numberToEnqueue, unsigned rank, unsigned bank)
{
    vector<BusPacket*>& queue = getCommandQueue(rank, bank);
    return ((cmd_queue_depth_ - queue.size()) >= numberToEnqueue);
}

                                           
void CommandQueue::print()
{
    if (queuingStructure_ == PerRank)
    {
        PRINT(endl << "== Printing Per Rank Queue");
        for (size_t i = 0; i < num_ranks_; i++)
        {
            PRINT(" = Rank " << i << "  size : " << queues[i][0].size());
            for (size_t j = 0; j < queues[i][0].size(); j++)
            {
                PRINTN("    " << j << "]");
                queues[i][0][j]->print();
            }
        }
    }
    else if (queuingStructure_ == PerRankPerBank)
    {
        PRINT("\n== Printing Per Rank, Per Bank Queue");

        for (size_t i = 0; i < num_ranks_; i++)
        {
            PRINT(" = Rank " << i);
            for (size_t j = 0; j < num_banks_; j++)
            {
                PRINT("    Bank " << j << "   size : " << queues[i][j].size());

                for (size_t k = 0; k < queues[i][j].size(); k++)
                {
                    PRINTN("       " << k << "]");
                    queues[i][j][k]->print();
                }
            }
        }
    }
}

   
                                                                   
                                                                     
                                                     
   
vector<BusPacket*>& CommandQueue::getCommandQueue(unsigned rank, unsigned bank)
{
    if (queuingStructure_ == PerRankPerBank)
        return queues[rank][bank];
    else if (queuingStructure_ == PerRank)
        return queues[rank][0];
    else
    {
        ERROR("Unknown queue structure");
        abort();
    }
}

                                              
bool CommandQueue::isIssuable(BusPacket* busPacket)
{
    switch (busPacket->busPacketType)
    {
        case REF:
        case RFCSB:
            return true;
            break;
        case ACTIVATE:

            if ((*ranks)[busPacket->rank]->mode_ != dramMode::SB && busPacket->bank >= 2)
            {
                return false;
            }

            if ((bankStates[busPacket->rank][busPacket->bank].currentBankState == Idle ||
                 bankStates[busPacket->rank][busPacket->bank].currentBankState == Refreshing) &&
                currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextActivate &&
                tXAWCountdown[busPacket->rank].size() < xaw_)
            {
                return true;
            }
            else
            {
                return false;
            }
            break;

        case WRITE:
            if (bankStates[busPacket->rank][busPacket->bank].currentBankState == RowActive &&
                currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextWrite &&
                busPacket->row == bankStates[busPacket->rank][busPacket->bank].openRowAddress &&
                rowAccessCounters[busPacket->rank][busPacket->bank] < total_row_accesses_)
            {
                return true;
            }
            else
            {
                return false;
            }
            break;
        case READ:
            if (bankStates[busPacket->rank][busPacket->bank].currentBankState == RowActive &&
                currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextRead &&
                busPacket->row == bankStates[busPacket->rank][busPacket->bank].openRowAddress &&
                rowAccessCounters[busPacket->rank][busPacket->bank] < total_row_accesses_)
            {
                return true;
            }
            else
            {
                return false;
            }
            break;
        case PRECHARGE:
            if (bankStates[busPacket->rank][busPacket->bank].currentBankState == RowActive &&
                currentClockCycle >= bankStates[busPacket->rank][busPacket->bank].nextPrecharge)
            {
                return true;
            }
            else
            {
                return false;
            }
            break;

        default:
            ERROR("== Error - Trying to issue a crazy bus packet type : ");
            busPacket->print();
            exit(0);
    }
    return false;
}

                                                                             
                                                                              
                                                                              
CommandQueue::TimingFlags CommandQueue::diagnoseTimingNoIssue() const
{
    TimingFlags f;
    if (queuingStructure_ != PerRank) return f;
    for (unsigned rank = 0; rank < num_ranks_; ++rank) {
        for (const BusPacket* p : queues[rank][0]) {
            const BankState& b = bankStates[p->rank][p->bank];
            if (p->busPacketType == READ || p->busPacketType == WRITE) {
                if (b.currentBankState != RowActive) {
                    f.wait_row_not_open = 1;
                    if (b.currentBankState != Idle) f.wait_bank_state = 1;
                    continue;
                }
                if (p->row != b.openRowAddress) { f.wait_wrong_row = 1; continue; }
                if (rowAccessCounters[p->rank][p->bank] >= total_row_accesses_) {
                    f.wait_row_access_limit = 1; continue;
                }
                const bool read = p->busPacketType == READ;
                const uint64_t gate = read ? b.nextRead : b.nextWrite;
                if (currentClockCycle >= gate) continue;
                const TimingOrigin origin = read ? b.nextReadOrigin : b.nextWriteOrigin;
                switch (origin) {
                    case TimingOrigin::ActivateToColumn: f.wait_tRCD = 1; break;
                    case TimingOrigin::ReadToRead:
                    case TimingOrigin::WriteToWrite: f.wait_tCCD_or_burst = 1; break;
                    case TimingOrigin::ReadToWrite:
                    case TimingOrigin::WriteToRead: f.wait_bus_turnaround = 1; break;
                    case TimingOrigin::RankTurnaround: f.wait_rank_turnaround = 1; break;
                    default: f.wait_unknown_gate = 1; break;
                }
            } else if (p->busPacketType == ACTIVATE) {
                if ((*ranks)[p->rank]->mode_ != dramMode::SB && p->bank >= 2)
                    f.wait_pim_bank_restricted = 1;
                if (b.currentBankState != Idle && b.currentBankState != Refreshing)
                    f.wait_bank_state = 1;
                if (tXAWCountdown[p->rank].size() >= xaw_) f.wait_tFAW = 1;
                if (currentClockCycle < b.nextActivate) {
                    switch (b.nextActivateOrigin) {
                        case TimingOrigin::ActivateToActivate: f.wait_tRRD = 1; break;
                        case TimingOrigin::RowCycle: f.wait_tRC = 1; break;
                        case TimingOrigin::PrechargeRecovery: f.wait_tRP = 1; break;
                        case TimingOrigin::RefreshRecovery: f.wait_tRFC = 1; break;
                        default: f.wait_unknown_gate = 1; break;
                    }
                }
            } else if (p->busPacketType == PRECHARGE) {
                if (b.currentBankState != RowActive) f.wait_bank_state = 1;
                if (currentClockCycle < b.nextPrecharge) {
                    switch (b.nextPrechargeOrigin) {
                        case TimingOrigin::ActivateToPrecharge: f.wait_tRAS = 1; break;
                        case TimingOrigin::ReadToPrecharge: f.wait_read_to_pre = 1; break;
                        case TimingOrigin::WriteToPrecharge: f.wait_write_to_pre = 1; break;
                        default: f.wait_unknown_gate = 1; break;
                    }
                }
            }
        }
    }
    return f;
}

                                                                          
                                                                                  
CommandQueue::WaitFlags CommandQueue::diagnoseNoIssue() const
{
    WaitFlags f;
    for (unsigned rank = 0; rank < num_ranks_; ++rank)
    {
                                                                                        
        const auto& queue = queues[rank][0];
        bool barrier_seen = false;
        for (size_t i = 0; i < queue.size(); ++i)
        {
            const auto* packet = queue[i];
            bool barrier = packet->tag.find("BAR") != std::string::npos;
            if (barrier_seen || (barrier && i != 0))
            {
                f.barrier = 1;
                break;
            }
            if (barrier) barrier_seen = true;
            bool dependent = false;
            for (size_t j = 0; j < i; ++j)
            {
                const auto* earlier = queue[j];
                if (earlier->tag.find("BAR") != std::string::npos ||
                    (packet->bank == earlier->bank && packet->row == earlier->row &&
                     packet->column == earlier->column))
                {
                    dependent = true;
                    break;
                }
            }
            if (dependent) { f.address_dependency = 1; continue; }
            const auto& bank = bankStates[packet->rank][packet->bank];
            switch (packet->busPacketType)
            {
                case READ:
                case WRITE:
                    if (bank.currentBankState == Idle || bank.currentBankState == Refreshing)
                    {
                        f.needs_activate = 1;
                        if (currentClockCycle < bank.nextActivate) f.activate_timing = 1;
                        if ((*ranks)[packet->rank]->mode_ != dramMode::SB && packet->bank >= 2)
                            f.pim_bank_restricted = 1;
                    }
                    else if (bank.currentBankState == RowActive)
                    {
                        if (packet->row != bank.openRowAddress) f.wrong_open_row = 1;
                        else if (rowAccessCounters[packet->rank][packet->bank] >= total_row_accesses_)
                            f.row_limit = 1;
                        else if (currentClockCycle < (packet->busPacketType == READ ? bank.nextRead : bank.nextWrite))
                            f.read_write_timing = 1;
                        else f.other = 1;
                    }
                    else f.other = 1;
                    break;
                case ACTIVATE:
                    if ((*ranks)[packet->rank]->mode_ != dramMode::SB && packet->bank >= 2)
                        f.pim_bank_restricted = 1;
                    else if (currentClockCycle < bank.nextActivate ||
                             tXAWCountdown[packet->rank].size() >= xaw_)
                        f.activate_timing = 1;
                    else f.other = 1;
                    break;
                case PRECHARGE:
                    if (currentClockCycle < bank.nextPrecharge) f.precharge_timing = 1;
                    else f.other = 1;
                    break;
                default: f.other = 1; break;
            }
                                                                                    
                                                                                 
            if (bank.currentBankState == RowActive &&
                packet->row != bank.openRowAddress &&
                currentClockCycle < bank.nextPrecharge)
                f.precharge_timing = 1;
        }
    }
    return f;
}

                                                                                  
                                                                          
                                                                             
                                                                                     
CommandQueue::CounterfactualFlags CommandQueue::diagnoseCounterfactualNoIssue() const
{
    CounterfactualFlags f;
    if (queuingStructure_ != PerRank) return f;

                                                                            
                                                                         
    const auto dramReady = [&](const BusPacket* p) -> bool {
        const auto& b = bankStates[p->rank][p->bank];
        switch (p->busPacketType)
        {
            case READ:
            case WRITE:
                return b.currentBankState == RowActive &&
                    p->row == b.openRowAddress &&
                    rowAccessCounters[p->rank][p->bank] < total_row_accesses_ &&
                    currentClockCycle >= (p->busPacketType == READ ? b.nextRead : b.nextWrite);
            case ACTIVATE:
                return ((*ranks)[p->rank]->mode_ == dramMode::SB || p->bank < 2) &&
                    (b.currentBankState == Idle || b.currentBankState == Refreshing) &&
                    currentClockCycle >= b.nextActivate &&
                    tXAWCountdown[p->rank].size() < xaw_;
            case PRECHARGE:
                return b.currentBankState == RowActive &&
                    currentClockCycle >= b.nextPrecharge;
            default:
                return false;                                                      
        }
    };

    bool any_ready = false;
    bool any_ready_not_barrier_or_dependency_blocked = false;
    for (unsigned rank = 0; rank < num_ranks_; ++rank)
    {
        const auto& queue = queues[rank][0];
        bool earlier_barrier = false;
        for (size_t i = 0; i < queue.size(); ++i)
        {
            const BusPacket* p = queue[i];
            const bool self_barrier = p->tag.find("BAR", 0) != std::string::npos;
            if (self_barrier) f.barrier_present = 1;
            if (!dramReady(p))
            {
                earlier_barrier |= self_barrier;
                continue;
            }
            any_ready = true;
            f.any_dram_ready_queued_command = 1;
            if (i == 0) f.head_dram_ready = 1;
            bool same_address_dependency = false;
            for (size_t j = 0; j < i; ++j)
            {
                const BusPacket* q = queue[j];
                if (p->bank == q->bank && p->row == q->row && p->column == q->column)
                {
                    same_address_dependency = true;
                    break;
                }
            }
            const bool barrier_order_block = earlier_barrier || (self_barrier && i != 0);
            if (barrier_order_block)
            {
                f.barrier_blocks_ready_command = 1;
                if (same_address_dependency)
                    f.ready_behind_barrier_with_dependency = 1;
            }
            else if (same_address_dependency)
                f.dependency_blocks_ready_command = 1;
            else
                any_ready_not_barrier_or_dependency_blocked = true;
            earlier_barrier |= self_barrier;
        }
    }
                                                                              
                                                                                
    f.timing_only_blocked = !any_ready;
                                                                               
                                                                               
    f.issuable_but_not_selected = any_ready_not_barrier_or_dependency_blocked;
    return f;
}

                                         
bool CommandQueue::isEmpty(unsigned rank)
{
    if (queuingStructure_ == PerRank)
    {
        return queues[rank][0].empty();
    }
    else if (queuingStructure_ == PerRankPerBank)
    {
        for (size_t i = 0; i < num_banks_; i++)
        {
            if (!queues[rank][i].empty())
                return false;
        }
        return true;
    }
    else
    {
        DEBUG("Invalid Queueing Stucture");
        abort();
    }
}

                                                                         
void CommandQueue::needRefresh(unsigned rank)
{
    refreshWaiting = true;
    refreshRank = rank;
}

void CommandQueue::nextRankAndBank(unsigned& rank, unsigned& bank)
{
    if (schedulingPolicy_ == RankThenBankRoundRobin)
    {
        rank++;
        if (rank == num_ranks_)
        {
            rank = 0;
            bank++;
            if (bank == num_banks_)
            {
                bank = 0;
            }
        }
    }
                                 
    else if (schedulingPolicy_ == BankThenRankRoundRobin)
    {
        bank++;
        if (bank == num_banks_)
        {
            bank = 0;
            rank++;
            if (rank == num_ranks_)
            {
                rank = 0;
            }
        }
    }
    else
    {
        ERROR("== Error - Unknown scheduling policy");
        exit(0);
    }
}

void CommandQueue::update()
{
                                                      
                                 
                                                    
}
