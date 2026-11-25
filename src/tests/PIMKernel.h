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

#ifndef __PIM_KERNEL_HPP__
#define __PIM_KERNEL_HPP__

#include <memory>
#include <set>
#include <tuple>
#include <sstream>
#include <string>
#include <vector>

#include "MultiChannelMemorySystem.h"
#include "PIMCmd.h"
#include "SystemConfiguration.h"
#include "tests/KernelAddrGen.h"

using namespace std;
using namespace DRAMSim;

class PIMKernel
{
  public:
    PIMKernel(shared_ptr<MultiChannelMemorySystem> mem, int num_pim_chan, int num_pim_rank)
        : mem_(mem),
          num_pim_chans_(num_pim_chan),
          num_pim_ranks_(num_pim_rank),
          mode_(PIMConfiguration::getPIMMode()),
          num_banks_(getConfigParam(UINT, "NUM_BANKS")),
          num_pim_blocks_(getConfigParam(UINT, "NUM_PIM_BLOCKS")),
          num_bank_groups_(getConfigParam(UINT, "NUM_BANK_GROUPS")),
          srf_bst_(NULL),
          cycle_(0)
    {
        transaction_size_ = getConfigParam(UINT, "BL") *
                            (getConfigParam(UINT, "JEDEC_DATA_BUS_BITS") / 8);            

                           
        num_grf_ = num_grfA_ = num_grfB_ = 8;
        num_total_pim_blocks_ = num_pim_blocks_ * num_pim_chans_ * num_pim_ranks_;

        pim_chans_.clear();
        for (int i = 0; i < num_pim_chans_; i++) pim_chans_.push_back(i);

        pim_ranks_.clear();
        for (int i = 0; i < num_pim_ranks_; i++) pim_ranks_.push_back(i);

        pim_addr_mgr_ = make_shared<PIMAddrManager>(num_pim_chan, num_pim_rank);
    }

    int transaction_size_;
    int num_pim_chans_, num_pim_ranks_;
    int num_grfA_, num_grfB_, num_grf_;
    shared_ptr<PIMAddrManager> pim_addr_mgr_;

    void addBarrier();
    void runPIM();
    uint64_t getCycle();
    void parkIn();
    void parkOut();
    void changePIMMode(dramMode mode1, dramMode mode2);
    void addTransactionAll(bool isWrite, int bg, int bank, int row, int col, const std::string tag,
                           BurstType* bst, bool use_barrier = false, int num_loop = 1);
    void addTransactionAll(bool isWrite, int bg, int bank, int row, int col, BurstType* bst,
                           bool use_barrier = false, int num_loop = 1);
      
                                                                            
                                                                              
                                                                             
                                                                                    
                                          
      
      
                      
      
    void programCrf(vector<PIMCmd>& cmds);
    void setControl(BurstType* bst, bool op, int crf_toggle_cond, bool grfA_zero, bool grfB_zero);
    unsigned getResultColGemv(int input_dim, int output_dim);
    void changeBank(pimBankType bank_types, int& cidx, int& rank, int& bg, int& bank,
                    unsigned& startingRow, unsigned& startingCol, unsigned& row, unsigned& col);
    void preloadGemv(NumpyBurstType* operand, unsigned starting_row = 0, unsigned starting_col = 0);
    void preloadNoReplacement(NumpyBurstType* operand, unsigned startingRow, unsigned startingCol);
      
                                                                                              
                                              
      
    void executeGemv(NumpyBurstType* w_data, NumpyBurstType* i_data, bool is_tree);
    void executeEltwise(int dim, pimBankType bank_types, KernelType ktype, int input0_row,
                        int result_row, int input1_row = 0);
                                                                           
    BurstType grfa_cache_control_payloads_[8] = {};
    bool step10_cache_enabled_ = false;
    uint64_t step10_fill_controls_ = 0;
    uint64_t step10_restore_controls_ = 0;
    uint64_t step10_skipped_grfa_writes_ = 0;
    void step10CacheControl(unsigned input_tile, bool restore);
    void computeGemv(NumpyBurstType* data, int num_input_tiles, int num_output_tile, int input_tile,
                     int output_tile, int batch_idx, pimBankType bank_types);
    void computeAddOrMul(int numTile, int input0Row, int resultRow, int input1Row);
    void computeRelu(int numTile, int input0Row, int resultRow);
                                                                 

    void readResult(BurstType* resultBst, pimBankType bank_types, int output_dim,
                    uint64_t baseAddr = 0, unsigned startingRow = 0, unsigned startingCol = 0);
    void readData(BurstType* bst_data, size_t bst_cnt, unsigned s_row = 0, unsigned s_col = 0);
    void adderTree(BurstType* result, int output_dim, int numTile, int step, fp16* temp);

  private:
                                                                        
                                                                                 
    struct GemvTileProfile {
        uint64_t output_tiles = 0, input_tiles = 0, batches = 0;
        uint64_t compute_tile_calls = 0;
        uint64_t grfa_write_transactions = 0, mac_read_transactions = 0;
        uint64_t grfa_barrier_calls = 0, mac_barrier_calls = 0;
        uint64_t grfb_result_transactions = 0, grfb_result_barriers = 0;
        uint64_t grfb_reset_transactions = 0, grfb_reset_barriers = 0;
        uint64_t pim_mode_entry_per_output_batch = 0;
        uint64_t pim_mode_exit_per_output_batch = 0;
        uint64_t crf_program_calls = 0;
        bool is_tree = false;
    } gemv_tile_;
                                                                                   
    struct GemvReuseProfile {
        uint64_t generated_grfa_loads = 0;
        uint64_t distinct_logical_inputs = 0;
        uint64_t repeated_logical_inputs = 0;
        uint64_t compute_tiles = 0;
        uint64_t even_compute_tiles = 0;
        uint64_t odd_compute_tiles = 0;
        uint64_t output_tiles = 0;
        uint64_t input_tiles = 0;
        uint64_t batches = 0;
        uint64_t distinct_input_tiles = 0;
        uint64_t repeated_input_tiles = 0;
        std::set<std::tuple<int,int,int,int,int>> seen_loads;
        std::set<std::tuple<int,int>> seen_tiles;
    } gemv_reuse_;
    unsigned cycle_;
    unsigned num_banks_, num_pim_blocks_, num_bank_groups_, num_total_pim_blocks_;
    BurstType null_bst_, bst_hab_pim_, bst_hab_;
    BurstType crf_bst_[4];
    BurstType* srf_bst_;
    vector<int> pim_chans_;
    vector<int> pim_ranks_;
    PIMMode mode_;
    shared_ptr<MultiChannelMemorySystem> mem_;
    const uint32_t pim_reg_ra_ = 0x3fff;
    const uint32_t pim_abmr_ra_ = 0x27ff;
    const uint32_t pim_sbmr_ra_ = 0x2fff;

    int inline getToggleCond(pimBankType pb_type = pimBankType::ALL_BANK)
    {
                               
        switch (pb_type)
        {
            case pimBankType::EVEN_BANK:
                return 2;
            case pimBankType::ODD_BANK:
                return 1;
            case pimBankType::ALL_BANK:
                return 0;
            default:
                return -1;
        }
    }
};

#endif
