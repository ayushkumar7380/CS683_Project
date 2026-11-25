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

#include "MemoryController.h"

#include <iostream>
#include <cstdlib>
#include <stdexcept>

#include "AddressMapping.h"
#include "MemorySystem.h"

#define SEQUENTIAL(rank, bank) (rank * config.NUM_BANKS) + bank

using namespace DRAMSim;

MemoryController::MemoryController(MemorySystem* parent, CSVWriter& csvOut_, ostream& simLog,
                                   Configuration& configuration)
    : dramsimLog(simLog),
      config(configuration),
      bankStates(getConfigParam(UINT, "NUM_RANKS"),
                 vector<BankState>(getConfigParam(UINT, "NUM_BANKS"), dramsimLog)),
      outgoingCmdPacket(NULL),
      outgoingDataPacket(NULL),
      dataCyclesLeft(0),
      cmdCyclesLeft(0),
      commandQueue(bankStates, simLog),
      poppedBusPacket(NULL),
      csvOut(csvOut_),
      totalTransactions(0),
      totalRefreshes(0),
      refreshRank(0),
      refreshBank(0),
      totalReads(0),
      totalWrites(0)
{
                           
    parentMemorySystem = parent;

    currentClockCycle = 0;
                                                                        
                                                                     
    if (const char* env = std::getenv("PIM_STEP13_RESTORE_EXTRA_CYCLES")) {
        char* end = nullptr;
        const long v = std::strtol(env, &end, 10);
        if (!*env || *end != '\0' || v < 0 || v > 10000)
            throw std::runtime_error("PIM_STEP13_RESTORE_EXTRA_CYCLES must be 0..10000");
        step13_extra_cycles_ = static_cast<unsigned>(v);
    }

    const size_t num_bank_entries = config.NUM_RANKS * config.NUM_BANKS;
    step3_per_bank.resize(num_bank_entries);
    step3_last_activated_row.assign(num_bank_entries, 0);
    step3_has_last_activated_row.assign(num_bank_entries, false);

                                 
    transactionQueue.reserve(config.TRANS_QUEUE_DEPTH);
    powerDown = vector<bool>(config.NUM_RANKS, false);

    grandTotalBankAccesses = totalReadsPerBank = totalWritesPerBank = totalActivatesPerBank =
        vector<uint64_t>(config.NUM_RANKS * config.NUM_BANKS, 0);
    totalReadsPerRank = totalWritesPerRank = totalActivatesPerRank =
        vector<uint64_t>(config.NUM_RANKS, 0);

    writeDataCountdown.reserve(config.NUM_RANKS);
    writeDataToSend.reserve(config.NUM_RANKS);
    refreshCountdown.reserve(config.NUM_RANKS);
    refreshCountdownBank.reserve(config.NUM_BANKS);

                            
    backgroundEnergy = burstEnergy = actpreEnergy = vector<uint64_t>(config.NUM_RANKS, 0);
    refreshEnergy = aluPIMEnergy = vector<uint64_t>(config.NUM_RANKS, 0);
    readPIMEnergy = vector<uint64_t>(config.NUM_BANKS, 0);
    totalBandwidth = 0.0;

    totalEpochLatency = vector<uint64_t>(config.NUM_RANKS * config.NUM_BANKS, 0);

                                                   
    for (size_t i = 0; i < config.NUM_RANKS; i++)
        refreshCountdown.push_back((int)((config.tREFI / config.tCK) / config.NUM_RANKS) * (i + 1));
    for (size_t i = 0; i < config.NUM_BANKS; i++)
        refreshCountdownBank.push_back((int)((config.tREFISB / config.tCK)) * (i + 1));

    memoryContStats = new MemoryControllerStats(
        parentMemorySystem, csvOut, dramsimLog, config, totalTransactions, grandTotalBankAccesses,
        totalReadsPerRank, totalWritesPerRank, totalReadsPerBank, totalWritesPerBank,
        totalActivatesPerRank, totalActivatesPerBank, totalRefreshes, backgroundEnergy, burstEnergy,
        actpreEnergy, refreshEnergy, aluPIMEnergy, refreshEnergy, pendingReadTransactions);
}

                                               
void MemoryController::receiveFromBus(BusPacket* bpacket)
{
    if (bpacket->busPacketType != DATA)
    {
        ERROR("== Error - Memory Controller received a non-DATA bus packet from rank");
        bpacket->print();
        exit(0);
    }

    if (DEBUG_BUS)
    {
        PRINTN(" -- MC Receiving From Data Bus : ");
        bpacket->print();
    }

                                    
    returnTransaction.push_back(
        new Transaction(RETURN_DATA, bpacket->physicalAddress, bpacket->data));

                                                                  
    delete (bpacket);
}

                                  
void MemoryController::returnReadData(const Transaction* trans)
{
    if (parentMemorySystem->ReturnReadData != NULL)
        (*parentMemorySystem->ReturnReadData)(parentMemorySystem->systemID, trans->address,
                                              currentClockCycle);
    parentMemorySystem->numOnTheFlyTransactions--;
}

bool MemoryController::addBarrier()
{
    if (transactionQueue.size())
    {
        transactionQueue.back()->tag += "BAR";
        return true;
    }
    return false;
}

                                                           
void MemoryController::attachRanks(vector<Rank*>* ranks)
{
    this->ranks = ranks;
    commandQueue.ranks = ranks;
}

void MemoryController::setBankStatesRW(size_t ra, size_t ba, uint64_t RdCycle, uint64_t WrCycle,
                                       TimingOrigin readOrigin, TimingOrigin writeOrigin)
{
    auto& b = bankStates[ra][ba];
    const uint64_t rd = currentClockCycle + RdCycle;
    const uint64_t wr = currentClockCycle + WrCycle;
    if (rd > b.nextRead) { b.nextRead = rd; b.nextReadOrigin = readOrigin; }
    if (wr > b.nextWrite) { b.nextWrite = wr; b.nextWriteOrigin = writeOrigin; }
}

void MemoryController::setBankStates(size_t rank, size_t bank, CurrentBankState currentBankState,
                                     BusPacketType lastCommand, uint64_t stateChangeCountdown,
                                     uint64_t nextActivate)
{
    bankStates[rank][bank].currentBankState = currentBankState;
    bankStates[rank][bank].lastCommand = lastCommand;
    if (stateChangeCountdown != 0)
        bankStates[rank][bank].stateChangeCountdown = stateChangeCountdown;
    const bool increased = nextActivate > bankStates[rank][bank].nextActivate;
    bankStates[rank][bank].nextActivate = nextActivate;
    if (increased || lastCommand == REF) {
        if (lastCommand == ACTIVATE) bankStates[rank][bank].nextActivateOrigin = TimingOrigin::RowCycle;
        if (lastCommand == PRECHARGE) bankStates[rank][bank].nextActivateOrigin = TimingOrigin::PrechargeRecovery;
        if (lastCommand == REF) bankStates[rank][bank].nextActivateOrigin = TimingOrigin::RefreshRecovery;
    }
}

void MemoryController::recordStep3Command(const BusPacket* packet)
{
                                                                             
                                                                           
    const unsigned rank = packet->rank;
    const unsigned bank = packet->bank;
    const size_t index = SEQUENTIAL(rank, bank);
    RowBankProfile& per = step3_per_bank[index];
    const bool is_pim = (*ranks)[rank]->mode_ == dramMode::HAB_PIM;
    const BankState& state = bankStates[rank][bank];

    switch (packet->busPacketType)
    {
        case READ:
        case WRITE:
        {
            const bool read = packet->busPacketType == READ;
            if (read) { ++per.column_reads; ++step3_aggregate.column_reads; }
            else { ++per.column_writes; ++step3_aggregate.column_writes; }
            if (is_pim) {
                if (read) { ++per.pim_column_reads; ++step3_aggregate.pim_column_reads; }
                else { ++per.pim_column_writes; ++step3_aggregate.pim_column_writes; }
            }
            if (state.currentBankState == RowActive && state.openRowAddress == packet->row) {
                ++per.row_hit_on_issue; ++step3_aggregate.row_hit_on_issue;
                if (is_pim) { ++per.pim_row_hit_on_issue; ++step3_aggregate.pim_row_hit_on_issue; }
            } else if (state.currentBankState != RowActive) {
                ++per.row_not_open_on_issue; ++step3_aggregate.row_not_open_on_issue;
            } else {
                ++per.wrong_row_on_issue; ++step3_aggregate.wrong_row_on_issue;
            }
            break;
        }
        case ACTIVATE:
        {
            ++per.activate; ++step3_aggregate.activate;
            if (step3_has_last_activated_row[index]) {
                if (step3_last_activated_row[index] == packet->row) {
                    ++per.activate_same_row; ++step3_aggregate.activate_same_row;
                } else {
                    ++per.activate_different_row; ++step3_aggregate.activate_different_row;
                }
            }
            step3_has_last_activated_row[index] = true;
            step3_last_activated_row[index] = packet->row;
            break;
        }
        case PRECHARGE:
            ++per.precharge; ++step3_aggregate.precharge;
            break;
        default: break;
    }

    if (packet->busPacketType == READ || packet->busPacketType == WRITE ||
        packet->busPacketType == ACTIVATE || packet->busPacketType == PRECHARGE) {
        uint64_t active_count = 0;
        for (unsigned b = 0; b < config.NUM_BANKS; ++b)
            if (bankStates[rank][b].currentBankState == RowActive) ++active_count;
        ++step3_aggregate.active_bank_samples;
        step3_aggregate.active_bank_sample_sum += active_count;
        if (active_count > step3_aggregate.active_bank_sample_max)
            step3_aggregate.active_bank_sample_max = active_count;
    }
}

void MemoryController::updateCommandQueue(BusPacket* poppedBusPacket)
{
    if (poppedBusPacket->busPacketType == WRITE)
    {
        writeDataToSend.push_back(new BusPacket(
            DATA, poppedBusPacket->physicalAddress, poppedBusPacket->column, poppedBusPacket->row,
            poppedBusPacket->rank, poppedBusPacket->bank, poppedBusPacket->data, dramsimLog));
        writeDataCountdown.push_back(config.WL);
    }

                                                                         
                                                      
    unsigned rank = poppedBusPacket->rank;
    unsigned bank = poppedBusPacket->bank;
    auto am = config.addrMapping;

    switch (poppedBusPacket->busPacketType)
    {
        case READ:
            if (currentClockCycle + config.READ_TO_PRE_DELAY > bankStates[rank][bank].nextPrecharge) {
                bankStates[rank][bank].nextPrecharge = currentClockCycle + config.READ_TO_PRE_DELAY;
                bankStates[rank][bank].nextPrechargeOrigin = TimingOrigin::ReadToPrecharge;
            }
            bankStates[rank][bank].lastCommand = READ;
            for (size_t i = 0; i < config.NUM_RANKS; i++)
            {
                for (size_t j = 0; j < config.NUM_BANKS; j++)
                {
                    if (i != poppedBusPacket->rank)
                    {
                        if (bankStates[i][j].currentBankState == RowActive)
                        {
                            setBankStatesRW(i, j, config.BL / 2 + config.tRTRS,
                                            config.READ_TO_WRITE_DELAY,
                                            TimingOrigin::RankTurnaround, TimingOrigin::RankTurnaround);
                        }
                    }
                    else
                    {
                        uint64_t RdCycle =
                            max((am.isSameBankgroup(j, bank) ? config.tCCDL : config.tCCDS),
                                config.BL / 2);
                        setBankStatesRW(i, j, RdCycle, config.READ_TO_WRITE_DELAY,
                                        TimingOrigin::ReadToRead, TimingOrigin::ReadToWrite);
                    }
                }
            }
            totalReads++;

            break;

        case WRITE:
            if (currentClockCycle + config.WRITE_TO_PRE_DELAY > bankStates[rank][bank].nextPrecharge) {
                bankStates[rank][bank].nextPrecharge = currentClockCycle + config.WRITE_TO_PRE_DELAY;
                bankStates[rank][bank].nextPrechargeOrigin = TimingOrigin::WriteToPrecharge;
            }
            bankStates[rank][bank].lastCommand = WRITE;
            for (size_t i = 0; i < config.NUM_RANKS; i++)
            {
                for (size_t j = 0; j < config.NUM_BANKS; j++)
                {
                    if (i != poppedBusPacket->rank)
                    {
                        if (bankStates[i][j].currentBankState == RowActive)
                        {
                            setBankStatesRW(i, j, config.WRITE_TO_READ_DELAY_R,
                                            config.BL / 2 + config.tRTRS,
                                            TimingOrigin::RankTurnaround, TimingOrigin::RankTurnaround);
                        }
                    }
                    else
                    {
                        uint64_t WrCycle =
                            max((am.isSameBankgroup(j, bank) ? config.tCCDL : config.tCCDS),
                                config.BL / 2);
                        setBankStatesRW(i, j, config.WRITE_TO_READ_DELAY_B_LONG, WrCycle,
                                        TimingOrigin::WriteToRead, TimingOrigin::WriteToWrite);
                    }
                }
            }
            totalWrites++;

            break;

        case ACTIVATE:
            setBankStates(rank, bank, RowActive, ACTIVATE, 0,
                          max(currentClockCycle + config.tRC, bankStates[rank][bank].nextActivate));
            bankStates[rank][bank].openRowAddress = poppedBusPacket->row;
            if (currentClockCycle + config.tRAS > bankStates[rank][bank].nextPrecharge) {
                bankStates[rank][bank].nextPrecharge = currentClockCycle + config.tRAS;
                bankStates[rank][bank].nextPrechargeOrigin = TimingOrigin::ActivateToPrecharge;
            }

                                                                                           
                        
            setBankStatesRW(rank, bank, (config.tRCDRD - config.AL), (config.tRCDWR - config.AL),
                            TimingOrigin::ActivateToColumn, TimingOrigin::ActivateToColumn);

            for (size_t i = 0; i < config.NUM_BANKS; i++)
            {
                if (i != poppedBusPacket->bank)
                {
                    const uint64_t deadline = currentClockCycle +
                        (am.isSameBankgroup(i, bank) ? config.tRRDL : config.tRRDS);
                    if (deadline > bankStates[rank][i].nextActivate) {
                        bankStates[rank][i].nextActivate = deadline;
                        bankStates[rank][i].nextActivateOrigin = TimingOrigin::ActivateToActivate;
                    }
                }
            }

            break;

        case PRECHARGE:
            setBankStates(rank, bank, Precharging, PRECHARGE, config.tRP,
                          max(currentClockCycle + config.tRP, bankStates[rank][bank].nextActivate));

            break;

        case REF:
            for (size_t i = 0; i < config.NUM_BANKS; i++)
                setBankStates(rank, i, Refreshing, REF, config.tRFC,
                              currentClockCycle + config.tRFC);

            break;
        default:
            ERROR("== Error - Popped a command we shouldn't have of type : "
                  << poppedBusPacket->busPacketType);
            exit(0);
    }

                                   
    if (DEBUG_BUS)
    {
        PRINTN(" -- MC Issuing On Command Bus : ");
        poppedBusPacket->print();
    }

                                 
    if (outgoingCmdPacket != NULL)
    {
        ERROR("== Error - Command Bus Collision");
        exit(-1);
    }
    outgoingCmdPacket = poppedBusPacket;
    cmdCyclesLeft = config.tCMD;
}

void MemoryController::updateTransactionQueue()
{
    auto am = config.addrMapping;
    for (size_t i = 0; i < transactionQueue.size(); i++)
    {
                                                                                      
                                            
        Transaction* transaction = transactionQueue[i];

                                           
        unsigned newTransactionChan, newTransactionRank, newTransactionBank, newTransactionRow,
            newTransactionColumn;

                                                                                     
        am.addressMapping(transaction->address, newTransactionChan, newTransactionRank,
                          newTransactionBank, newTransactionRow, newTransactionColumn);

                                                                                  
                                            
        if (commandQueue.hasRoomFor(1, newTransactionRank, newTransactionBank))
        {
            if (DEBUG_ADDR_MAP)
            {
                PRINTN("== New Transaction - Mapping Address [0x" << hex << transaction->address
                                                                  << dec << "]");
                if (transaction->transactionType == DATA_READ)
                    PRINT(" (Read)");
                else
                    PRINT(" (Write)");
                PRINT("  Rank : " << newTransactionRank);
                PRINT("  Bank : " << newTransactionBank);
                PRINT("  Row  : " << newTransactionRow);
                PRINT("  Col  : " << newTransactionColumn);
            }

                                                                                      
                                    
            transactionQueue.erase(transactionQueue.begin() + i);
            BusPacket* command;

                                                          
            BusPacketType bpType = transaction->getBusPacketType();
            command = new BusPacket(bpType, transaction->address, newTransactionColumn,
                                    newTransactionRow, newTransactionRank, newTransactionBank,
                                    transaction->data, dramsimLog);
            command->tag = transaction->tag;
                                                                             
                                                                             
                                                                            
            {
                const size_t index = SEQUENTIAL(newTransactionRank, newTransactionBank);
                const BankState& bs = bankStates[newTransactionRank][newTransactionBank];
                ++step3_per_bank[index].enqueued_column_requests;
                ++step3_aggregate.enqueued_column_requests;
                if (bs.currentBankState == RowActive) {
                    if (bs.openRowAddress == newTransactionRow) {
                        ++step3_per_bank[index].enqueue_row_open_hit;
                        ++step3_aggregate.enqueue_row_open_hit;
                    } else {
                        ++step3_per_bank[index].enqueue_wrong_open_row;
                        ++step3_aggregate.enqueue_wrong_open_row;
                    }
                } else {
                    ++step3_per_bank[index].enqueue_row_not_open;
                    ++step3_aggregate.enqueue_row_not_open;
                }
            }
            commandQueue.enqueue(command);

                                                                                  
                                                                                      
            if (transaction->transactionType == DATA_READ)
                pendingReadTransactions.push_back(transaction);
            else
                                                                        
                delete transaction;
                                                                           
                     
                                                                             
                                                                             
                                                      
               
            break;
        }
          
                                              
         
                                                                      
         
          
    }
}

void MemoryController::printDebugOnUpate()
{
    if (DEBUG_TRANS_Q)
    {
        PRINT("== Printing transaction queue");
        for (size_t i = 0; i < transactionQueue.size(); i++)
            PRINTN("  " << i << "] " << *transactionQueue[i]);
    }

    if (DEBUG_BANKSTATE)
    {
        PRINT("== Printing bank states (According to MC)");
        for (size_t i = 0; i < config.NUM_RANKS; i++)
        {
            for (size_t j = 0; j < config.NUM_BANKS; j++)
            {
                if (bankStates[i][j].currentBankState == RowActive)
                    PRINTN("[" << bankStates[i][j].openRowAddress << "] ");
                else if (bankStates[i][j].currentBankState == Idle)
                    PRINTN("[idle] ");
                else if (bankStates[i][j].currentBankState == Precharging)
                    PRINTN("[pre] ");
                else if (bankStates[i][j].currentBankState == Refreshing)
                    PRINTN("[ref] ");
                else if (bankStates[i][j].currentBankState == PowerDown)
                    PRINTN("[lowp] ");
            }
            PRINT("");                                 
        }
    }

    if (DEBUG_CMD_Q)
        commandQueue.print();
}

void MemoryController::updateBankState()
{
    for (size_t i = 0; i < config.NUM_RANKS; i++)
    {
        for (size_t j = 0; j < config.NUM_BANKS; j++)
        {
            if (bankStates[i][j].stateChangeCountdown > 0)
            {
                                     
                bankStates[i][j].stateChangeCountdown--;

                                                         
                if (bankStates[i][j].stateChangeCountdown == 0)
                {
                    switch (bankStates[i][j].lastCommand)
                    {
                        case REF:
                        case RFCSB:
                        case PRECHARGE:
                            bankStates[i][j].currentBankState = Idle;
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
}

void MemoryController::updateRefresh()
{
    if (refreshCountdown[refreshRank] == 0)
    {
        commandQueue.needRefresh(refreshRank);
        (*ranks)[refreshRank]->refreshWaiting = true;
                                                                                 
        refreshCountdown[refreshRank] = config.tREFI / config.tCK;
        refreshRank++;
        if (refreshRank == config.NUM_RANKS)
            refreshRank = 0;
    }
                                                                                
    else if (powerDown[refreshRank] && refreshCountdown[refreshRank] <= config.tXP)
        (*ranks)[refreshRank]->refreshWaiting = true;
}

void MemoryController::update()
{
    ++baseline_cycles;
    updateBankState();
                                                               
    if (outgoingCmdPacket != NULL)
    {
        cmdCyclesLeft--;
        if (cmdCyclesLeft == 0)                                           
        {
            (*ranks)[outgoingCmdPacket->rank]->receiveFromBus(outgoingCmdPacket);
            outgoingCmdPacket = NULL;
        }
    }

                                                            
    if (outgoingDataPacket != NULL)
    {
        dataCyclesLeft--;
        if (dataCyclesLeft == 0)
        {
                                                       
            if (parentMemorySystem->WriteDataDone != NULL)
            {
                (*parentMemorySystem->WriteDataDone)(parentMemorySystem->systemID,
                                                     outgoingDataPacket->physicalAddress,
                                                     currentClockCycle);
            }
            parentMemorySystem->numOnTheFlyTransactions--;
            (*ranks)[outgoingDataPacket->rank]->receiveFromBus(outgoingDataPacket);
            outgoingDataPacket = NULL;
        }
    }

                                                     
                                                         
                            
      
                                                           
    if (writeDataCountdown.size() > 0)
    {
        for (size_t i = 0; i < writeDataCountdown.size(); i++) writeDataCountdown[i]--;

        if (writeDataCountdown[0] == 0)
        {
                                                
            if (DEBUG_BUS)
            {
                PRINTN(" -- MC Issuing On Data Bus    : ");
                writeDataToSend[0]->print();
            }

                                             
            if (outgoingDataPacket != NULL)
            {
                ERROR("== Error - Data Bus Collision");
                exit(-1);
            }

            outgoingDataPacket = writeDataToSend[0];
            dataCyclesLeft = config.BL / 2;

            totalTransactions++;

            writeDataCountdown.erase(writeDataCountdown.begin());
            writeDataToSend.erase(writeDataToSend.begin());
        }
    }

                                                
                                                    
    updateRefresh();

                                          
                                                                           
    const bool step13_stalling = step13_cooldown_remaining_ > 0;
    if (step13_stalling) {
        --step13_cooldown_remaining_;
        ++step13_cooldown_cycles_;
                                                                         
                                                                        
        for (unsigned r = 0; r < config.NUM_RANKS; ++r) {
            if (!commandQueue.isEmpty(r)) {
                ++step13_cooldown_with_queue_;
                break;
            }
        }
    }
    if (!step13_stalling && commandQueue.pop(&poppedBusPacket))
    {
        switch (poppedBusPacket->busPacketType)
        {
            case READ:
                ++baseline_issued_read_commands;
                if ((*ranks)[poppedBusPacket->rank]->mode_ == dramMode::HAB_PIM)
                    ++baseline_issued_pim_mode_read_commands;
                break;
            case WRITE:
                ++baseline_issued_write_commands;
                if ((*ranks)[poppedBusPacket->rank]->mode_ == dramMode::HAB_PIM)
                    ++baseline_issued_pim_mode_write_commands;
                break;
            case ACTIVATE: ++baseline_issued_activate_commands; break;
            case PRECHARGE: ++baseline_issued_precharge_commands; break;
            case REF: ++baseline_issued_refresh_commands; break;
            default: break;
        }
                                                                
                                                                         
                                                                    
                                                                           
                                                                         
        if (poppedBusPacket->busPacketType == WRITE &&
            poppedBusPacket->row == config.PIM_REG_RA &&
            poppedBusPacket->column == 0x11) {
            ++step13_restore_issued_;
            step13_cooldown_remaining_ = step13_extra_cycles_;
        }
        recordStep5IssuedCommand(poppedBusPacket);
        recordStep6IssuedCommand(poppedBusPacket);
        recordStep3Command(poppedBusPacket);
        updateCommandQueue(poppedBusPacket);
    }
    else
    {
                                                                               
        for (unsigned rank = 0; rank < config.NUM_RANKS; ++rank)
        {
            if (!commandQueue.isEmpty(rank))
            {
                ++baseline_queued_no_issue_cycles;
                const auto tf = commandQueue.diagnoseTimingNoIssue();
                baseline_timing.wait_tRCD += tf.wait_tRCD;
                baseline_timing.wait_tCCD_or_burst += tf.wait_tCCD_or_burst;
                baseline_timing.wait_bus_turnaround += tf.wait_bus_turnaround;
                baseline_timing.wait_rank_turnaround += tf.wait_rank_turnaround;
                baseline_timing.wait_tRP += tf.wait_tRP;
                baseline_timing.wait_tRAS += tf.wait_tRAS;
                baseline_timing.wait_tRRD += tf.wait_tRRD;
                baseline_timing.wait_tRC += tf.wait_tRC;
                baseline_timing.wait_tRFC += tf.wait_tRFC;
                baseline_timing.wait_tFAW += tf.wait_tFAW;
                baseline_timing.wait_read_to_pre += tf.wait_read_to_pre;
                baseline_timing.wait_write_to_pre += tf.wait_write_to_pre;
                baseline_timing.wait_row_not_open += tf.wait_row_not_open;
                baseline_timing.wait_wrong_row += tf.wait_wrong_row;
                baseline_timing.wait_bank_state += tf.wait_bank_state;
                baseline_timing.wait_pim_bank_restricted += tf.wait_pim_bank_restricted;
                baseline_timing.wait_row_access_limit += tf.wait_row_access_limit;
                baseline_timing.wait_unknown_gate += tf.wait_unknown_gate;
                const auto cf = commandQueue.diagnoseCounterfactualNoIssue();
                baseline_counterfactual.barrier_present += cf.barrier_present;
                baseline_counterfactual.barrier_blocks_ready_command += cf.barrier_blocks_ready_command;
                baseline_counterfactual.dependency_blocks_ready_command += cf.dependency_blocks_ready_command;
                baseline_counterfactual.timing_only_blocked += cf.timing_only_blocked;
                baseline_counterfactual.issuable_but_not_selected += cf.issuable_but_not_selected;
                baseline_counterfactual.ready_behind_barrier_with_dependency += cf.ready_behind_barrier_with_dependency;
                baseline_counterfactual.any_dram_ready_queued_command += cf.any_dram_ready_queued_command;
                baseline_counterfactual.head_dram_ready += cf.head_dram_ready;
                const auto f = commandQueue.diagnoseNoIssue();
                baseline_waits.barrier += f.barrier;
                baseline_waits.address_dependency += f.address_dependency;
                baseline_waits.read_write_timing += f.read_write_timing;
                baseline_waits.needs_activate += f.needs_activate;
                baseline_waits.wrong_open_row += f.wrong_open_row;
                baseline_waits.row_limit += f.row_limit;
                baseline_waits.activate_timing += f.activate_timing;
                baseline_waits.pim_bank_restricted += f.pim_bank_restricted;
                baseline_waits.precharge_timing += f.precharge_timing;
                baseline_waits.other += f.other;
                break;
            }
        }
    }

    updateTransactionQueue();

                                                      
    if (returnTransaction.size() > 0)
    {
        if (DEBUG_BUS)
            PRINTN(" -- MC Issuing to CPU bus : " << *returnTransaction[0]);
        totalTransactions++;

        bool foundMatch = false;
                                                                 
        for (size_t i = 0; i < pendingReadTransactions.size(); i++)
        {
            if (pendingReadTransactions[i]->address == returnTransaction[0]->address)
            {
                unsigned chan, rank, bank, row, col;
                config.addrMapping.addressMapping(returnTransaction[0]->address, chan, rank, bank,
                                                  row, col);
                memoryContStats->insertHistogram(
                    currentClockCycle - pendingReadTransactions[i]->timeAdded, rank, bank);
                                        
                                                           
                                                                                      
                returnReadData(pendingReadTransactions[i]);

                delete pendingReadTransactions[i];
                pendingReadTransactions.erase(pendingReadTransactions.begin() + i);
                foundMatch = true;
                break;
            }
        }
        if (!foundMatch)
        {
            ERROR("Can't find a matching transaction for 0x" << hex << returnTransaction[0]->address
                                                             << dec);
            abort();
        }
        delete returnTransaction[0];
        returnTransaction.erase(returnTransaction.begin());
    }

                                 
    for (size_t i = 0; i < config.NUM_RANKS; i++) refreshCountdown[i]--;
    for (size_t i = 0; i < config.NUM_BANKS; i++) refreshCountdownBank[i]--;

                  
    printDebugOnUpate();

    commandQueue.step();
}

bool MemoryController::WillAcceptTransaction()
{
    return transactionQueue.size() < getConfigParam(UINT, "TRANS_QUEUE_DEPTH");
}

                                                         
bool MemoryController::addTransaction(Transaction* trans)
{
    if (WillAcceptTransaction())
    {
        parentMemorySystem->numOnTheFlyTransactions++;
        trans->timeAdded = currentClockCycle;
        transactionQueue.push_back(trans);
        return true;
    }
    else
    {
        return false;
    }
}

                                                                           
                                                                                       
void MemoryController::recordStep5IssuedCommand(const BusPacket* p)
{
    auto& c = step5_stream_;
    ++c.issued;
    const bool column = p->busPacketType == READ || p->busPacketType == WRITE;
    const bool pim = (*ranks)[p->rank]->mode_ == dramMode::HAB_PIM;
    const bool barrier = p->tag.find("BAR") != std::string::npos;
    const bool mac = p->tag.find("MAC") != std::string::npos;
    if (!p->tag.empty()) ++c.tagged;
    if (barrier) { ++c.barrier_tagged; if (column) ++c.barrier_tagged_columns; }
    if (mac) { ++c.mac_tagged; if (pim) ++c.mac_pim_mode; }
    if (p->tag.find("CRF") != std::string::npos) ++c.crf_tagged;
    if (p->tag.find("GRF") != std::string::npos) ++c.grf_tagged;
    if (p->tag.find("HAB") != std::string::npos || p->tag.find("SB") != std::string::npos)
        ++c.mode_tagged;
    if (!p->tag.empty() && !mac && !barrier &&
        p->tag.find("CRF") == std::string::npos &&
        p->tag.find("GRF") == std::string::npos &&
        p->tag.find("HAB") == std::string::npos &&
        p->tag.find("SB") == std::string::npos) ++c.other_tagged;
    if (column) {
        if (pim) ++c.pim_mode_column_commands;
        if (step5_prev_column_) {
            if (p->rank == step5_prev_rank_) {
                ++c.consecutive_same_rank_columns;
                if (p->bank == step5_prev_bank_)
                    ++c.consecutive_same_bank_columns;
                else ++c.consecutive_other_bank_columns;
            } else ++c.consecutive_other_bank_columns;
        } else ++c.column_streams;
        step5_prev_column_ = true;
        step5_prev_rank_ = p->rank;
        step5_prev_bank_ = p->bank;
    } else step5_prev_column_ = false;
    if (mac) {
        if (!step5_prev_mac_) ++c.mac_runs;
        ++step5_current_mac_run_;
        if (step5_current_mac_run_ > c.max_mac_run) c.max_mac_run = step5_current_mac_run_;
    } else step5_current_mac_run_ = 0;
    step5_prev_mac_ = mac;

    if (std::getenv("PIM_TRACE_COMMANDS") && parentMemorySystem->systemID == 0 &&
        step5_trace_count_ < 160) {
        const char* type = p->busPacketType == READ ? "READ" :
            p->busPacketType == WRITE ? "WRITE" :
            p->busPacketType == ACTIVATE ? "ACT" :
            p->busPacketType == PRECHARGE ? "PRE" :
            p->busPacketType == REF ? "REF" : "OTHER";
        std::cout << "[PIM_CMD_TRACE] cycle=" << currentClockCycle
                  << " channel=0 type=" << type << " rank=" << p->rank
                  << " bank=" << p->bank << " row=" << p->row
                  << " col=" << p->column << " pim_mode=" << pim
                  << " barrier_tag=" << barrier << " tag=" << p->tag << "\n";
        ++step5_trace_count_;
    }
}

                                                                   
MemoryController::PIMPhaseProfile MemoryController::getPIMPhaseProfile() const
{
    PIMPhaseProfile out = step6_phase_;
    const uint64_t issued = step5_stream_.issued;
    if (step6_seen_mac_) {
        out.phase_setup_commands = step6_first_mac_issued_index_ - 1;
        out.phase_compute_commands = step6_last_mac_issued_index_ - step6_first_mac_issued_index_ + 1;
        out.phase_teardown_commands = issued - step6_last_mac_issued_index_;
        out.phase_first_to_last_mac_cycles = step6_last_mac_cycle_ - step6_first_mac_cycle_ + 1;
        out.phase_first_to_last_mac_valid = 1;
        if (step6_in_run_) {
            ++out.mac_runs_completed;
            out.mac_run_length_sum += step6_run_length_;
        }
    } else {
        out.phase_setup_commands = issued;
    }
    return out;
}

void MemoryController::recordStep6IssuedCommand(const BusPacket* p)
{
    auto& c = step6_phase_;
    const std::string& tag = p->tag;
    const bool mac = tag.find("MAC") != std::string::npos;
    const bool grf = tag.find("GRF") != std::string::npos;
    const bool barrier = tag.find("BAR") != std::string::npos;
    const bool crf = tag.find("CRF") != std::string::npos;
    const bool mode = tag.find("HAB") != std::string::npos ||
                      tag.find("SB") != std::string::npos;
    const bool actpre = p->busPacketType == ACTIVATE || p->busPacketType == PRECHARGE;
    if (barrier) {
        if (mac) ++c.barrier_tagged_mac;
        else if (grf) ++c.barrier_tagged_grf;
        else ++c.barrier_tagged_other;
    }
    if (grf) {
        if (tag.find("TO_GRF") != std::string::npos || tag.find("RESET_GRF") != std::string::npos)
            ++c.grf_load_tagged;
        if (tag.find("GRF_TO_") != std::string::npos || tag.find("GRFB_TO_") != std::string::npos ||
            tag.find("GRF_A_TO_") != std::string::npos || tag.find("GRF_B_TO_") != std::string::npos)
            ++c.grf_store_tagged;
    }
    if (mac) {
        if (!step6_seen_mac_) {
            step6_seen_mac_ = true;
            step6_first_mac_cycle_ = currentClockCycle;
            step6_first_mac_issued_index_ = step5_stream_.issued;
        }
        if (!step6_in_run_) {
            if (step6_gap_commands_ != 0) {
                ++c.mac_gap_events;
                c.mac_gap_commands += step6_gap_commands_;
                if (step6_gap_commands_ > c.max_mac_gap_commands)
                    c.max_mac_gap_commands = step6_gap_commands_;
                                                                                         
                const uint64_t gap_cycles = currentClockCycle - step6_last_mac_cycle_ - 1;
                c.mac_gap_cycles += gap_cycles;
                if (gap_cycles > c.max_mac_gap_cycles) c.max_mac_gap_cycles = gap_cycles;
                c.grf_before_mac_run += step6_gap_grf_;
                c.barrier_before_mac_run += step6_gap_barrier_;
            }
            step6_gap_commands_ = step6_gap_grf_ = step6_gap_barrier_ = 0;
            step6_in_run_ = true;
            step6_run_length_ = 0;
            step6_prev_mac_bank_valid_ = false;
        }
        ++step6_run_length_;
        if (step6_prev_mac_bank_valid_ &&
            (step6_prev_mac_rank_ != p->rank || step6_prev_mac_bank_ != p->bank))
            ++c.mac_run_bank_switches;
        step6_prev_mac_rank_ = p->rank;
        step6_prev_mac_bank_ = p->bank;
        step6_prev_mac_bank_valid_ = true;
        step6_last_mac_cycle_ = currentClockCycle;
        step6_last_mac_issued_index_ = step5_stream_.issued;
    } else if (step6_seen_mac_) {
        if (step6_in_run_) {
            ++c.mac_runs_completed;
            c.mac_run_length_sum += step6_run_length_;
            step6_in_run_ = false;
            step6_gap_commands_ = step6_gap_grf_ = step6_gap_barrier_ = 0;
                                                                         
            if (grf) ++c.mac_break_grf;
            else if (barrier) ++c.mac_break_barrier;
            else if (crf) ++c.mac_break_crf;
            else if (mode) ++c.mac_break_mode;
            else if (actpre) ++c.mac_break_actpre;
            else ++c.mac_break_other;
        }
        ++step6_gap_commands_;
        if (grf) { ++c.gap_grf; ++step6_gap_grf_; }
        if (barrier) { ++c.gap_barrier; ++step6_gap_barrier_; }
        if (crf) ++c.gap_crf;
        if (mode) ++c.gap_mode;
        if (actpre) ++c.gap_actpre;
        if (!grf && !barrier && !crf && !mode && !actpre) ++c.gap_other;
    }
                                                                                     
    if (std::getenv("PIM_TRACE_MAC_PHASE") && parentMemorySystem->systemID == 0 &&
        step6_seen_mac_ && step6_trace_count_ < 240) {
        const char* typ = p->busPacketType == READ ? "READ" :
                          p->busPacketType == WRITE ? "WRITE" :
                          p->busPacketType == ACTIVATE ? "ACT" :
                          p->busPacketType == PRECHARGE ? "PRE" : "OTHER";
        std::cout << "[PIM_MAC_PHASE_TRACE] cycle=" << currentClockCycle
                  << " channel=0 type=" << typ << " rank=" << p->rank
                  << " bank=" << p->bank << " mac=" << mac
                  << " grf=" << grf << " barrier=" << barrier
                  << " tag=" << tag << "\n";
        ++step6_trace_count_;
    }
}

void MemoryController::resetStats()
{
    memoryContStats->resetStats();
}

MemoryController::BaselineCommandCounters MemoryController::getBaselineCounters() const
{
    return {baseline_cycles, baseline_queued_no_issue_cycles,
            baseline_issued_read_commands, baseline_issued_write_commands,
            baseline_issued_activate_commands, baseline_issued_precharge_commands,
            baseline_issued_refresh_commands, baseline_issued_pim_mode_read_commands,
            baseline_issued_pim_mode_write_commands, baseline_waits, baseline_counterfactual, baseline_timing, step3_aggregate,
            commandQueue.getSelectionOpportunity()};
}

                                                         
void MemoryController::printStats(bool finalStats)
{
    memoryContStats->printStats(finalStats, parentMemorySystem->systemID, currentClockCycle);
}

MemoryController::~MemoryController()
{
                                             
               
    for (size_t i = 0; i < pendingReadTransactions.size(); i++) delete pendingReadTransactions[i];
    for (size_t i = 0; i < returnTransaction.size(); i++) delete returnTransaction[i];
    delete memoryContStats;
}

                                               
void MemoryControllerStats::insertHistogram(unsigned latencyValue, unsigned rank, unsigned bank)
{
    totalEpochLatency[SEQUENTIAL(rank, bank)] += latencyValue;
                                    
    unsigned histogram_bin_size = getConfigParam(UINT, "HISTOGRAM_BIN_SIZE");
    latencies[(latencyValue / histogram_bin_size) * histogram_bin_size]++;
}

void MemoryControllerStats::printStats(bool finalStats, unsigned myChannel,
                                       uint64_t currentClockCycle)
{
                                                                                          
                     
    uint64_t cyclesElapsed = (currentClockCycle % config.EPOCH_LENGTH == 0)
                                 ? config.EPOCH_LENGTH
                                 : currentClockCycle % config.EPOCH_LENGTH;
    unsigned bytesPerTransaction = (getConfigParam(UINT, "JEDEC_DATA_BUS_BITS") * config.BL) / 8;
    uint64_t totalBytesTransferred = totalTransactions * bytesPerTransaction;
    double secondsThisEpoch = (double)cyclesElapsed * config.tCK * 1E-9;

                    
    vector<double> backgroundPower = vector<double>(config.NUM_RANKS, 0.0);
    vector<double> burstPower = vector<double>(config.NUM_RANKS, 0.0);
    vector<double> refreshPower = vector<double>(config.NUM_RANKS, 0.0);
    vector<double> actprePower = vector<double>(config.NUM_RANKS, 0.0);
    vector<double> averagePower = vector<double>(config.NUM_RANKS, 0.0);
    vector<double> aluPIMPower = vector<double>(config.NUM_RANKS, 0.0);

                         
    vector<double> averageLatency = vector<double>(config.NUM_RANKS * config.NUM_BANKS, 0.0);
    vector<double> bandwidth = vector<double>(config.NUM_RANKS * config.NUM_BANKS, 0.0);

    for (size_t i = 0; i < config.NUM_RANKS; i++)
    {
        for (size_t j = 0; j < config.NUM_BANKS; j++)
        {
            bandwidth[SEQUENTIAL(i, j)] = (((double)(totalReadsPerBank[SEQUENTIAL(i, j)] +
                                                     totalWritesPerBank[SEQUENTIAL(i, j)]) *
                                            (double)bytesPerTransaction) /
                                           (1024.0 * 1024.0 * 1024.0)) /
                                          secondsThisEpoch;
            averageLatency[SEQUENTIAL(i, j)] = ((float)totalEpochLatency[SEQUENTIAL(i, j)] /
                                                (float)(totalReadsPerBank[SEQUENTIAL(i, j)])) *
                                               config.tCK;
            totalBandwidth += bandwidth[SEQUENTIAL(i, j)];
            totalReadsPerRank[i] += totalReadsPerBank[SEQUENTIAL(i, j)];
            totalWritesPerRank[i] += totalWritesPerBank[SEQUENTIAL(i, j)];
            totalActivatesPerRank[i] += totalActivatesPerBank[SEQUENTIAL(i, j)];
        }
    }
    LOG_OUTPUT ? dramsimLog.setf(ios::fixed, ios::floatfield)
               : cout.setf(ios::fixed, ios::floatfield);

    PRINTC(PRINT_CHAN_STAT, " =======================================================");
    PRINTC(PRINT_CHAN_STAT, " ============== Printing Statistics [id:"
                                << parentMemorySystem->systemID << "]==============");
    PRINTNC(PRINT_CHAN_STAT, "   Total Return Transactions : " << totalTransactions);
    PRINTC(PRINT_CHAN_STAT, " (" << totalBytesTransferred << " bytes) aggregate average bandwidth "
                                 << totalBandwidth << "GB/s");
    PRINTC(PRINT_CHAN_STAT, "   Total Refreshes : " << totalRefreshes);

    double totalAggregateBandwidth = 0.0;
    for (size_t r = 0; r < config.NUM_RANKS; r++)
    {
        PRINTC(PRINT_CHAN_STAT, "      -Rank   " << r << " : ");
        PRINTNC(PRINT_CHAN_STAT, "        -Reads  : " << totalReadsPerRank[r]);
        PRINTC(PRINT_CHAN_STAT, " (" << totalReadsPerRank[r] * bytesPerTransaction << " bytes)");
        PRINTNC(PRINT_CHAN_STAT, "        -Writes : " << totalWritesPerRank[r]);
        PRINTC(PRINT_CHAN_STAT, " (" << totalWritesPerRank[r] * bytesPerTransaction << " bytes)");
        PRINTC(PRINT_CHAN_STAT, "        -Activates  : " << totalActivatesPerRank[r]);
                                                                               

        for (size_t j = 0; j < config.NUM_BANKS; j++)
        {
            PRINTC(PRINT_CHAN_STAT, "        -Bandwidth / Latency  (Bank "
                                        << j << "): " << bandwidth[SEQUENTIAL(r, j)] << " GB/s\t\t"
                                        << averageLatency[SEQUENTIAL(r, j)] << " ns");
        }

        burstPower[r] = ((double)burstEnergy[r] / (double)(cyclesElapsed)) / 1000.0;         
        actprePower[r] = ((double)actpreEnergy[r] / (double)(cyclesElapsed)) / 1000.0;
        aluPIMPower[r] = ((double)aluPIMEnergy[r] / (double)cyclesElapsed) / 1000.0;
        averagePower[r] = burstPower[r] + actprePower[r] + aluPIMPower[r];

        if ((*parentMemorySystem->ReportPower) != NULL)
            (*parentMemorySystem->ReportPower)(backgroundPower[r], burstPower[r], refreshPower[r],
                                               actprePower[r]);

        PRINTC(PRINT_CHAN_STAT, " == Power Data for Rank        " << r);
        PRINTC(PRINT_CHAN_STAT, "   Average Power (watts)     : " << averagePower[r]);
        PRINTC(PRINT_CHAN_STAT, "     -Act/Pre    (watts)     : " << actprePower[r]);
        PRINTC(PRINT_CHAN_STAT, "     -Burst      (watts)     : " << burstPower[r]);
        PRINTC(PRINT_CHAN_STAT, "     -AluPIM     (watts)     : " << aluPIMPower[r]);

        if (VIS_FILE_OUTPUT)
        {
                                        
            csvOut << CSVWriter::IndexedName("ACT_PRE_Power", myChannel, r) << actprePower[r];
            csvOut << CSVWriter::IndexedName("Burst_Power", myChannel, r) << burstPower[r];
            double totalRankBandwidth = 0.0;
            for (size_t b = 0; b < config.NUM_BANKS; b++)
            {
                csvOut << CSVWriter::IndexedName("Bandwidth", myChannel, r, b)
                       << bandwidth[SEQUENTIAL(r, b)];
                totalRankBandwidth += bandwidth[SEQUENTIAL(r, b)];
                totalAggregateBandwidth += bandwidth[SEQUENTIAL(r, b)];
                csvOut << CSVWriter::IndexedName("Average_Latency", myChannel, r, b)
                       << averageLatency[SEQUENTIAL(r, b)];
            }
            csvOut << CSVWriter::IndexedName("Rank_Aggregate_Bandwidth", myChannel, r)
                   << totalRankBandwidth;
            csvOut << CSVWriter::IndexedName("Rank_Average_Bandwidth", myChannel, r)
                   << totalRankBandwidth / config.NUM_RANKS;
        }
    }

    if (VIS_FILE_OUTPUT)
    {
        csvOut << CSVWriter::IndexedName("Aggregate_Bandwidth", myChannel)
               << totalAggregateBandwidth;
        csvOut << CSVWriter::IndexedName("Average_Bandwidth", myChannel)
               << totalAggregateBandwidth / (config.NUM_RANKS * config.NUM_BANKS);
    }

                                                                             
                                                     
    if (finalStats)
    {
        PRINTC(PRINT_CHAN_STAT, " ---  Latency list (" << latencies.size() << ")");
        PRINTC(PRINT_CHAN_STAT, "       [lat] : #");
        if (VIS_FILE_OUTPUT)
            csvOut.getOutputStream() << "!!HISTOGRAM_DATA" << endl;

        map<unsigned, unsigned>::iterator it;
        for (it = latencies.begin(); it != latencies.end(); it++)
        {
            auto histogram_bin_size = getConfigParam(UINT, "HISTOGRAM_BIN_SIZE");
            PRINTC(PRINT_CHAN_STAT, "       [" << it->first << "-"
                                               << it->first + (histogram_bin_size - 1)
                                               << "] : " << it->second);
            if (VIS_FILE_OUTPUT)
                csvOut.getOutputStream() << it->first << "=" << it->second << endl;
        }
        if (currentClockCycle > 0 && currentClockCycle % config.EPOCH_LENGTH == 0)
        {
            PRINTC(PRINT_CHAN_STAT, " --- Grand Total Bank usage list");
            for (size_t i = 0; i < config.NUM_RANKS; i++)
            {
                PRINTC(PRINT_CHAN_STAT, "Rank " << i << ":");
                for (size_t j = 0; j < config.NUM_BANKS; j++)
                {
                    PRINTC(PRINT_CHAN_STAT,
                           "  b" << j << ": " << grandTotalBankAccesses[SEQUENTIAL(i, j)]);
                }
            }
        }
    }

    PRINTC(PRINT_CHAN_STAT, endl << " == Pending Transactions : " << pendingReadTransactions.size()
                                 << " (" << currentClockCycle << ")==");

    if (LOG_OUTPUT)
        dramsimLog.flush();

                             
                    
}

void MemoryControllerStats::resetStats()
{
    totalRefreshes = 0;
    totalBandwidth = 0;
    for (size_t i = 0; i < config.NUM_RANKS; i++)
    {
        for (size_t j = 0; j < config.NUM_BANKS; j++)
        {
                                                                                
            grandTotalBankAccesses[SEQUENTIAL(i, j)] +=
                (totalReadsPerBank[SEQUENTIAL(i, j)] + totalWritesPerBank[SEQUENTIAL(i, j)]);
            totalReadsPerBank[SEQUENTIAL(i, j)] = 0;
            totalWritesPerBank[SEQUENTIAL(i, j)] = 0;
            totalActivatesPerBank[SEQUENTIAL(i, j)] = 0;
            totalEpochLatency[SEQUENTIAL(i, j)] = 0;
        }
        burstEnergy[i] = 0;
        actpreEnergy[i] = 0;
        refreshEnergy[i] = 0;
        backgroundEnergy[i] = 0;
        aluPIMEnergy[i] = 0;
        totalReadsPerRank[i] = 0;
        totalWritesPerRank[i] = 0;
        totalActivatesPerRank[i] = 0;
    }
}
