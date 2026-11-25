/***************************************************************************************************
 * Copyright (C) 2021 Samsung Electronics Co. LTD
 *
 * This software is a property of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed,
 * transmitted, transcribed, stored in a retrieval system, or translated into any human
 * or computer language in any form by any means,electronic, mechanical, manual or otherwise,
 * or disclosed to third parties without the express written permission of Samsung Electronics.
 * (Use of the Software is restricted to non-commercial, personal or academic, research purpose
 * only)
 **************************************************************************************************/

#ifndef _PIMRANK_H_
#define _PIMRANK_H_

#include <vector>
#include <cstdint>
#include <climits>
#include <unordered_set>

#include "AddressMapping.h"
#include "BusPacket.h"
#include "Configuration.h"
#include "PIMBlock.h"
#include "PIMCmd.h"
#include "Rank.h"
#include "SimulatorObject.h"

using namespace std;
using namespace DRAMSim;

namespace DRAMSim
{
#define OUTLOG_ALL(msg)                                                                       \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] bg["                         \
        << config.addrMapping.bankgroupId(packet->bank) << "] ba[" << packet->bank << "] ro[" \
        << packet->row << "] co[" << packet->column << "] @" << currentClockCycle
#define OUTLOG_CH_RA(msg) \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] @" << currentClockCycle
#define OUTLOG_PRECHARGE(msg)                                                                 \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] bg["                         \
        << config.addrMapping.bankgroupId(packet->bank) << "] ba[" << packet->bank << "] ro[" \
        << bankStates[packet->bank].openRowAddress << "] @" << currentClockCycle
#define OUTLOG_GRF_A(msg)                                                                 \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] pb[" << packet->bank / 2 \
        << " reg" << packet->column - 0x8 << " @" << currentClockCycle
#define OUTLOG_GRF_B(msg)                                                                 \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] pb[" << packet->bank / 2 \
        << "] reg[" << packet->column - 0x18 << "] @" << currentClockCycle
#define OUTLOG_B_GRF_A(msg)                                                                    \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] reg[" << packet->column - 0x8 \
        << "] @" << currentClockCycle
#define OUTLOG_B_GRF_B(msg)                                                                     \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] reg[" << packet->column - 0x18 \
        << "] @" << currentClockCycle
#define OUTLOG_B_CRF(msg)                                                                      \
    msg << " ch[" << getChanId() << "] ra[" << getRankId() << "] idx[" << packet->column - 0x4 \
        << "] @" << currentClockCycle

class Rank;                        

class PIMRank : public SimulatorObject
{
  private:
    int chanId;
    int rankId;
    ostream& dramsimLog;
    Configuration& config;
    int pimPC_, lastJumpIdx_, numJumpToBeTaken_, lastRepeatIdx_, numRepeatToBeDone_;
    bool pimOpMode_, toggleEvenBank_, toggleOddBank_, toggleRa13h_, crfExit_;

  public:
    PIMRank(ostream& simLog, Configuration& configuration);
    ~PIMRank() {}

    void attachRank(Rank* r);
    int getChanId() const;
    void setChanId(int id);
    int getRankId() const;
    void setRankId(int id);
                                                                                      
                                                                 
    struct BaselineCounters {
        uint64_t pim_mode_cycles = 0;
        uint64_t mac_issue_cycles = 0;
        uint64_t mac_instructions = 0;
        uint64_t bank_operand_reads = 0;
        uint64_t nonmac_pim_instructions = 0;
    } baseline;
                                                                               
    struct GrfAReuseCounters {
        uint64_t grfa_broadcast_writes = 0;
        uint64_t grfa_same_value_overwrites = 0;
        uint64_t grfa_changed_value_overwrites = 0;
        uint64_t grfa_overwrites_before_read = 0;
        uint64_t grfa_mac_operand_reads = 0;
        uint64_t grfa_mac_reads_before_load = 0;
        uint64_t grfa_mac_reads_after_load = 0;
        uint64_t grfa_mac_reuses_since_load = 0;
        uint64_t grfa_zeroizations = 0;
        uint64_t grfa_loaded_not_read_at_end = 0;                                            
    } grfa_reuse;
                                                                
    std::unordered_set<uint64_t> grfa_unique_payload_hashes;
    uint64_t grfa_zero_payload_writes = 0;
    uint64_t grfa_trace_writes = 0;
    bool grfa_seen_write[8] = {};
    bool grfa_read_since_write[8] = {};
    uint64_t grfa_reads_since_write[8] = {};
                                                              
                                                                              
    static constexpr unsigned kGrfaCacheTiles = 8;
    static constexpr unsigned kGrfaCacheSlots = 8;
    BurstType grfa_tile_cache[kGrfaCacheTiles][kGrfaCacheSlots] = {};
    bool grfa_tile_valid[kGrfaCacheTiles][kGrfaCacheSlots] = {};
    int grfa_cache_fill_tile = -1;
    uint64_t grfa_cache_selects = 0;
    uint64_t grfa_cache_restores = 0;
    uint64_t grfa_cache_invalid_restores = 0;
    uint64_t last_mac_issue_cycle = UINT64_MAX;
    void update();
    void readHab(BusPacket* packet);
    void writeHab(BusPacket* packet);
    void doPIM(BusPacket* packet);
    void doPIMBlock(BusPacket* packet, PIMCmd curCmd, int pimblock_id);
    void controlPIM(BusPacket* packet);
    void readOpd(int pb, BurstType& bst, PIMOpdType type, BusPacket* packet, int idx, bool is_auto,
                 bool is_mac);
    void writeOpd(int pb, BurstType& bst, PIMOpdType type, BusPacket* packet, int idx, bool is_auto,
                  bool is_mac);
    bool isToggleCond(BusPacket* packet);

    union crf_t
    {
        uint32_t data[32];
        BurstType bst[4];
        crf_t()
        {
            memset(data, 0, sizeof(uint32_t) * 32);
        }
    } crf;

    unsigned inline getGrfIdx(unsigned idx)
    {
        return idx & 0x7;
    }
    unsigned inline getGrfIdxHigh(unsigned r, unsigned c)
    {
        return ((r & 0x1) << 2 | ((c >> 3) & 0x3));
    }
    unsigned inline isReservedRA(unsigned row)
    {
        return (row & (1 << 13));
    }
    unsigned inline masked2accessibleRA(unsigned row)
    {
        return (row & ((1 << 13) - 1));
    }

    Rank* rank;
    vector<PIMBlock> pimBlocks;
};
}                      
#endif
