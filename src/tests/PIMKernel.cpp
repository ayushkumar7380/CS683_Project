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

#include "tests/PIMKernel.h"

#include <iomanip>
#include <cstdlib>
#include <string>

#include "AddressMapping.h"
#include "tests/PIMCmdGen.h"

void PIMKernel::runPIM()
{
    while (mem_->hasPendingTransactions())
    {
        cycle_++;
        mem_->update();
    }
}

uint64_t PIMKernel::getCycle()
{
    return cycle_;
}

void PIMKernel::parkIn()
{
    addBarrier();
    for (int& ch_idx : pim_chans_)
    {
        for (int& ra_idx : pim_ranks_)
        {
            for (int bank_idx = 0; bank_idx < num_banks_ / num_bank_groups_; bank_idx++)
            {
                for (int bg_idx = 0; bg_idx < num_bank_groups_; bg_idx++)
                {
                    string str = "PARK_IN_";
                    if (bg_idx == 0 && bank_idx == 0)
                        str = "START_" + str;
                    else if (bg_idx == 3 && bank_idx == 3)
                        str = "END_" + str;
                    mem_->addTransaction(
                        false,
                        pim_addr_mgr_->addrGen(ch_idx, ra_idx, bg_idx, bank_idx, (1 << 13), 0), str,
                        &null_bst_);
                }
            }
        }
    }
    addBarrier();
}

void PIMKernel::parkOut()
{
    for (int& ch_idx : pim_chans_)
    {
        for (int& ra_idx : pim_ranks_)
        {
            for (int bank_idx = 0; bank_idx < num_banks_ / num_bank_groups_; bank_idx++)
            {
                for (int bg_idx = 0; bg_idx < num_bank_groups_; bg_idx++)
                {
                    string str = "PARK_OUT_";
                    if (bg_idx == 0 && bank_idx == 0)
                        str = "START_" + str;
                    else if (bg_idx == 3 && bank_idx == 3)
                        str = "END_" + str;
                    mem_->addTransaction(
                        false,
                        pim_addr_mgr_->addrGen(ch_idx, ra_idx, bg_idx, bank_idx, (1 << 13), 0), str,
                        &null_bst_);
                }
            }
        }
    }
    addBarrier();
}

void PIMKernel::addTransactionAll(bool is_write, int bg_idx, int bank_idx, int row, int col,
                                  const string tag, BurstType* bst, bool use_barrier, int num_loop)
{
    for (int& ch_idx : pim_chans_)
        for (int& ra_idx : pim_ranks_)
        {
            unsigned local_row = row;
            unsigned local_col = col;
            for (int i = 0; i < num_loop; i++)
            {
                uint64_t addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx, bank_idx,
                                                           local_row, local_col);
                (tag != "") ? mem_->addTransaction(is_write, addr, tag, bst)
                            : mem_->addTransaction(is_write, addr, bst);
                local_col++;
            }
        }

    if (use_barrier)
        addBarrier();
}

void PIMKernel::addTransactionAll(bool is_write, int bg_idx, int bank_idx, int row, int col,
                                  BurstType* bst, bool use_barrier, int num_loop)
{
    addTransactionAll(is_write, bg_idx, bank_idx, row, col, "", bst, use_barrier, num_loop);
}

void PIMKernel::addBarrier()
{
    for (int& ch_idx : pim_chans_) mem_->addBarrier(ch_idx);
}

void PIMKernel::changePIMMode(dramMode curMode, dramMode nextMode)
{
    if (curMode == dramMode::SB && nextMode == dramMode::HAB)
    {
        addTransactionAll(true, 0, 0, pim_abmr_ra_, 0x1f, "START_SB_TO_HAB_", &null_bst_);
        addTransactionAll(true, 0, 1, pim_abmr_ra_, 0x1f, &null_bst_);
        if (num_banks_ >= 2)
        {
            addTransactionAll(true, 2, 0, pim_abmr_ra_, 0x1f, &null_bst_);
            addTransactionAll(true, 2, 1, pim_abmr_ra_, 0x1f, "END_SB_TO_HAB_", &null_bst_);
        }
    }
    else if (curMode == dramMode::HAB)
    {
        if (nextMode == dramMode::SB)
        {
            addTransactionAll(true, 0, 0, pim_sbmr_ra_, 0x1f, "START_HAB_TO_SB", &null_bst_);
            addTransactionAll(true, 0, 1, pim_sbmr_ra_, 0x1f, "END_HAB_TO_SB", &null_bst_);
        }
        else if (nextMode == dramMode::HAB_PIM)
        {
            addTransactionAll(true, 0, 0, pim_reg_ra_, 0x0, "PIM", &bst_hab_pim_);
        }
    }
    else if (curMode == dramMode::HAB_PIM && nextMode == dramMode::HAB)
        addTransactionAll(true, 0, 0, pim_reg_ra_, 0x0, "PIM", &bst_hab_);

    addBarrier();
}

  
                                                                                   
                                                                                     
                                                                                   
 
                                                    
     
                                                                                            
                                                                       
                                                                        
                                                                                        
                                                                      
     
 

                                                                                              
                                                                                           
                                                
 
                   
                   
                      
                           
                                                              

                                                                         
     
                                                                         
                                         
                                                                             
                                         
                                                                             
                                         
                                                                             
                                        

                 
                                     
         
                       
                     
         
                                     
         
                       
                                        
         
                           
         
                                                        
         
     
 

                            
 
                                                           
     
                                                               
         
                                      
                                                                                                
                                                                              
         
     
                 
 
  

void PIMKernel::programCrf(vector<PIMCmd>& cmds)
{
    PIMCmd nop_cmd(PIMCmdType::NOP, 0);
    for (int i = 0; i < 4; i++)
    {
        if (i * 8 >= cmds.size())
            break;
        crf_bst_[i].set(nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt(),
                        nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt(), nop_cmd.toInt());
        for (int j = 0; j < 8; j++)
        {
            if (i * 8 + j >= cmds.size())
                break;
            crf_bst_[i].u32Data_[j] = cmds[i * 8 + j].toInt();
        }
        addTransactionAll(true, 0, 1, pim_reg_ra_, 0x4 + i, "PROGRAM_CRF", &(crf_bst_[i]));
    }
    addBarrier();
}

void PIMKernel::setControl(BurstType* bst, bool pim_op, int crf_toggle_cond, bool grfA_zero,
                           bool grfB_zero)
{
    bst->u8Data_[0] = pim_op;
    bst->u8Data_[16] = crf_toggle_cond;
    bst->u8Data_[20] = grfA_zero;
    bst->u8Data_[21] = grfB_zero;
}

unsigned PIMKernel::getResultColGemv(int input_dim, int output_dim)
{
    int num_output_tiles = ceil(((double)output_dim / (num_total_pim_blocks_)) / num_grfB_);
    int num_input_tiles = ceil((double)input_dim / (double)num_grfA_);

    return num_output_tiles * num_input_tiles / 2 * num_grfA_ * num_grfB_;
}

void PIMKernel::changeBank(pimBankType pb_type, int& ch_idx, int& ra_idx, int& bg_idx,
                           int& bank_idx, unsigned& starting_row, unsigned& starting_col,
                           unsigned& row, unsigned& col)
{
    bank_idx += (pb_type == pimBankType::ALL_BANK) ? 1 : (num_banks_ / num_pim_blocks_);

    if (bank_idx >= (num_banks_ / num_bank_groups_))
    {
        bank_idx = 0;
        if (++bg_idx >= num_bank_groups_)
        {
            bg_idx = 0;
            if (++ra_idx >= num_pim_ranks_)
            {
                ra_idx = 0;
                if (++ch_idx >= num_pim_chans_)
                {
                    ch_idx = 0;
                    starting_row = row;
                    starting_col = col;
                }
            }
        }
    }
}

void PIMKernel::preloadGemv(NumpyBurstType* operand, unsigned starting_row, unsigned starting_col)
{
    int input_tile_size = num_grfA_;
    int output_tile_size = num_grfB_ * num_total_pim_blocks_;

    int ch_idx = 0, ra_idx = 0, bg_idx = 0, bank_idx = 0;
    unsigned row = 0, col = 0;
    uint64_t addr;

    unsigned even_starting_row = starting_row, odd_starting_row = starting_row;
    unsigned even_starting_col = starting_col, odd_starting_col = starting_col;

    for (int y = 0; y < operand->bShape[0]; y += output_tile_size)
    {
        for (int x = 0; x < operand->bShape[1]; x += input_tile_size)
        {
            bool is_odd = ((x / input_tile_size) % 2 == 1) ? true : false;

            for (int tiled_y = 0; tiled_y < output_tile_size; tiled_y += num_grfB_)
            {
                row = (is_odd) ? odd_starting_row : even_starting_row;
                col = (is_odd) ? odd_starting_col : even_starting_col;

                for (int grfb_idx = 0; grfb_idx < num_grfB_; grfb_idx++)
                {
                    for (int grfa_idx = 0; grfa_idx < num_grfA_; grfa_idx++, col++)
                    {
                        addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx, bank_idx + is_odd,
                                                          row, col);
                        int d_idx = (y + tiled_y + grfb_idx) * operand->bShape[1] + x + grfa_idx;
                        mem_->addTransaction(true, addr, &operand->bData[d_idx]);
                    }
                }
                is_odd ? changeBank(pimBankType::ODD_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                    odd_starting_row, odd_starting_col, row, col)
                       : changeBank(pimBankType::EVEN_BANK, ch_idx, ra_idx, bg_idx, bank_idx,
                                    even_starting_row, even_starting_col, row, col);
            }
        }
    }
}

void PIMKernel::preloadNoReplacement(NumpyBurstType* operand, unsigned starting_row,
                                     unsigned starting_col)
{
    uint64_t init_addr = pim_addr_mgr_->addrGenSafe(0, 0, 0, 0, starting_row, starting_col);

    for (int x = 0; x < operand->getTotalDim(); x++)
    {
        uint64_t addr = init_addr + x * transaction_size_;
        mem_->addTransaction(true, addr, &operand->bData[x]);
    }
}
  
                                                                            
                                                                           
 
                  
                  
                  
                    
                                       
                    
                                            

                                              
    
                                   
                                   

                                                           
        
                                                                                                    
                                                     
                                                                             
                 
        
                                                                                                   
    
 
  
void PIMKernel::step10CacheControl(unsigned input_tile, bool restore)
{
                                                                             
                                                                            
    for (int ch_idx = 0; ch_idx < num_pim_chans_; ++ch_idx)
        for (int ra_idx = 0; ra_idx < num_pim_ranks_; ++ra_idx)
            mem_->addTransaction(true,
                pim_addr_mgr_->addrGen(ch_idx, ra_idx, 0, 1, pim_reg_ra_,
                                      restore ? 0x11 : 0x10),
                restore ? "STEP10_RESTORE_GRFA" : "STEP10_SELECT_GRFA",
                &grfa_cache_control_payloads_[input_tile]);
    addBarrier();
    if (restore) ++step10_restore_controls_;
    else ++step10_fill_controls_;
}

void PIMKernel::executeGemv(NumpyBurstType* w_data, NumpyBurstType* i_data, bool is_tree)
{
    int num_output_tiles = ceil(((double)w_data->bShape[0] / (num_total_pim_blocks_)) / num_grfB_);
    int num_input_tiles = ceil((double)w_data->bShape[1] / (double)num_grfA_);
    int num_batch = i_data->bShape[0];
    int zero_row = 1000;
                                                                       
                                                              
    const char* enable = std::getenv("PIM_GEMV_TILE_CACHE");
    step10_cache_enabled_ = enable && std::string(enable) == "1" &&
        !is_tree && num_batch == 1 && num_output_tiles > 1 &&
        num_input_tiles > 0 && num_input_tiles <= 8;
    step10_fill_controls_ = step10_restore_controls_ =
        step10_skipped_grfa_writes_ = 0;
    for (int t = 0; t < 8; ++t)
    {
        grfa_cache_control_payloads_[t] = BurstType{};
        grfa_cache_control_payloads_[t].u8Data_[0] = uint8_t(t);
    }
    std::cout << (step10_cache_enabled_ ? "[CACHE_ENABLED_CONFIG]" : "[BASELINE_PIM_CONFIG]") << " requested=" << (enable ? enable : "0")
              << " enabled=" << step10_cache_enabled_
              << " output_tiles=" << num_output_tiles
              << " input_tiles=" << num_input_tiles
              << " batches=" << num_batch
              << " hardware_extension=grfa_tile_sram_2KB_per_rank"
              << std::endl;

                                                                   
    gemv_reuse_ = GemvReuseProfile{};
    gemv_reuse_.output_tiles = num_output_tiles;
    gemv_reuse_.input_tiles = num_input_tiles;
    gemv_reuse_.batches = num_batch;
                                                                               
    gemv_tile_ = GemvTileProfile{};
    gemv_tile_.output_tiles = num_output_tiles;
    gemv_tile_.input_tiles = num_input_tiles;
    gemv_tile_.batches = num_batch;
    gemv_tile_.is_tree = is_tree;

    if (is_tree)
    {
        for (int ch = 0; ch < num_pim_chans_; ch++)
        {
            for (int bg_idx = 0; bg_idx < num_bank_groups_; bg_idx++)
            {
                for (int ba = 0; ba < num_banks_ / num_bank_groups_; ba++)
                {
                    for (int ca = 0; ca < num_grfA_; ca++)
                    {
                        uint64_t addr = pim_addr_mgr_->addrGen(ch, 0, bg_idx, ba, zero_row, ca);
                        mem_->addTransaction(true, addr, &null_bst_);
                    }
                }
            }
        }
    }

    vector<PIMCmd> pim_cmds;
    if (is_tree)
    {
        int num_jump = ceil((double)num_input_tiles / 2) - 1;
        pim_cmds = PIMCmdGen::getPIMCmds(KernelType::GEMVTREE, num_jump, 0, 0);
    }
    else
    {
        int num_jump_of_even_bank = num_grfB_ * ceil((double)num_input_tiles / 2) - 1;
        int num_jump_of_odd_bank = num_grfB_ * floor(num_input_tiles / 2) - 1;
        pim_cmds =
            PIMCmdGen::getPIMCmds(KernelType::GEMV, 0, num_jump_of_odd_bank, num_jump_of_even_bank);
    }
    setControl(&bst_hab_pim_, true, getToggleCond(), false, true);
    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB);
    programCrf(pim_cmds);

                                                                      
    gemv_tile_.crf_program_calls = 1;
    for (int j = 0; j < num_output_tiles; j++)
    {
        for (int b = 0; b < num_batch; b++)
        {
            ++gemv_tile_.pim_mode_entry_per_output_batch;
            changePIMMode(dramMode::HAB, dramMode::HAB_PIM);              

            int col = num_output_tiles * num_input_tiles / 2 * num_grfA_ * num_grfB_ +
                      (j + b) * num_grfB_;
            if (is_tree)
            {
                for (int i = 0; i < num_input_tiles; i++, col += num_grfB_)
                {
                    computeGemv(i_data, num_input_tiles, num_output_tiles, i, j, b,
                                (i % 2 == 0) ? pimBankType::EVEN_BANK : pimBankType::ODD_BANK);
                    gemv_tile_.grfb_result_transactions +=
                        uint64_t(num_pim_chans_) * num_pim_ranks_ * num_grf_;
                    gemv_tile_.grfb_result_barriers += num_pim_chans_;
                    gemv_tile_.grfb_reset_transactions +=
                        uint64_t(num_pim_chans_) * num_pim_ranks_ * num_grfB_;
                    gemv_tile_.grfb_reset_barriers += num_pim_chans_;
                    addTransactionAll(true, 0, 1, 0, col, "GRFB_TO_BANK_", &null_bst_, true,
                                      num_grf_);
                    addTransactionAll(false, 0, 0, zero_row, 0, "RESET_GRF_B", &null_bst_, true,
                                      num_grfB_);
                }
            }
            else
            {
                for (int i = 0; i < num_input_tiles; i += 2)
                    computeGemv(i_data, num_input_tiles, num_output_tiles, i, j, b,
                                pimBankType::EVEN_BANK);
                for (int i = 1; i < num_input_tiles; i += 2)
                    computeGemv(i_data, num_input_tiles, num_output_tiles, i, j, b,
                                pimBankType::ODD_BANK);
                gemv_tile_.grfb_result_transactions +=
                    uint64_t(num_pim_chans_) * num_pim_ranks_ * num_grf_;
                gemv_tile_.grfb_result_barriers += num_pim_chans_;
                addTransactionAll(true, 0, 1, 0, col, "GRFB_TO_BANK_", &null_bst_, true, num_grf_);
            }
            ++gemv_tile_.pim_mode_exit_per_output_batch;
            changePIMMode(dramMode::HAB_PIM, dramMode::HAB);                  
        }
    }
    changePIMMode(dramMode::HAB, dramMode::SB);
    parkOut();
                                                                             
                                                                                
    std::cout << "[GEMV_TILE_PROFILE]"
              << " output_tiles=" << gemv_tile_.output_tiles
              << " input_tiles=" << gemv_tile_.input_tiles
              << " batches=" << gemv_tile_.batches
              << " is_tree=" << gemv_tile_.is_tree
              << " compute_tile_calls=" << gemv_tile_.compute_tile_calls
              << " grfa_write_transactions=" << gemv_tile_.grfa_write_transactions
              << " mac_read_transactions=" << gemv_tile_.mac_read_transactions
              << " grfa_barrier_calls=" << gemv_tile_.grfa_barrier_calls
              << " mac_barrier_calls=" << gemv_tile_.mac_barrier_calls
              << " grfb_result_transactions=" << gemv_tile_.grfb_result_transactions
              << " grfb_result_barriers=" << gemv_tile_.grfb_result_barriers
              << " grfb_reset_transactions=" << gemv_tile_.grfb_reset_transactions
              << " grfb_reset_barriers=" << gemv_tile_.grfb_reset_barriers
              << " pim_mode_entry_per_output_batch=" << gemv_tile_.pim_mode_entry_per_output_batch
              << " pim_mode_exit_per_output_batch=" << gemv_tile_.pim_mode_exit_per_output_batch
              << " crf_program_calls=" << gemv_tile_.crf_program_calls
              << " note=generated_not_issued_not_cycle_timing"
              << "\n";
    std::cout << (step10_cache_enabled_ ? "[CACHE_ENABLED_PROFILE]" : "[BASELINE_PIM_PROFILE]") << " enabled=" << step10_cache_enabled_
              << " fill_control_groups=" << step10_fill_controls_
              << " restore_control_groups=" << step10_restore_controls_
              << " skipped_grfa_transactions=" << step10_skipped_grfa_writes_
              << " cache_bytes_per_rank=2048"
              << " note=additional_hardware_extension_not_standard_fimdram"
              << std::endl;
    std::cout << "[GEMV_GRF_GENERATOR]"
              << " output_tiles=" << gemv_reuse_.output_tiles
              << " input_tiles=" << gemv_reuse_.input_tiles
              << " batches=" << gemv_reuse_.batches
              << " compute_tiles=" << gemv_reuse_.compute_tiles
              << " even_compute_tiles=" << gemv_reuse_.even_compute_tiles
              << " odd_compute_tiles=" << gemv_reuse_.odd_compute_tiles
              << " generated_grfa_loads=" << gemv_reuse_.generated_grfa_loads
              << " distinct_logical_inputs=" << gemv_reuse_.distinct_logical_inputs
              << " repeated_logical_inputs=" << gemv_reuse_.repeated_logical_inputs
              << " distinct_input_tiles=" << gemv_reuse_.distinct_input_tiles
              << " repeated_input_tiles=" << gemv_reuse_.repeated_input_tiles
              << "\n";
}

void PIMKernel::computeGemv(NumpyBurstType* data, int num_input_tiles, int num_output_tiles,
                            int inputTile, int outputTile, int batchIdx, pimBankType pb_type)
{
    ++gemv_tile_.compute_tile_calls;
                                                                                   
                                                                   
    if (inputTile == 0 && batchIdx == 0 && outputTile <= 1)
        std::cout << "[GEMV_TILE_SAMPLE] output_tile=" << outputTile
                  << " input_tile=" << inputTile
                  << " batch=" << batchIdx
                  << " bank_type=" << ((pb_type == pimBankType::EVEN_BANK) ? "even" : "odd")
                  << " grfa_writes_per_tile="
                  << uint64_t(num_pim_chans_) * num_pim_ranks_ * num_grfA_
                  << " mac_reads_per_tile="
                  << uint64_t(num_pim_chans_) * num_pim_ranks_ * num_grfA_ * 8
                  << "\n";
    ++gemv_reuse_.compute_tiles;
    if (pb_type == pimBankType::EVEN_BANK) ++gemv_reuse_.even_compute_tiles;
    if (pb_type == pimBankType::ODD_BANK) ++gemv_reuse_.odd_compute_tiles;
    if (gemv_reuse_.seen_tiles.insert({batchIdx, inputTile}).second)
        ++gemv_reuse_.distinct_input_tiles;
    else
        ++gemv_reuse_.repeated_input_tiles;
                                                                          
                                                                    
    if (step10_cache_enabled_)
    {
        step10CacheControl(unsigned(inputTile), outputTile > 0);
        if (outputTile > 0)
        {
            step10_skipped_grfa_writes_ +=
                uint64_t(num_pim_chans_) * num_pim_ranks_ * num_grfA_;
        }
    }
    if (!(step10_cache_enabled_ && outputTile > 0))
    for (int ch_idx = 0; ch_idx < num_pim_chans_; ch_idx++)
    {
        for (int ra_idx = 0; ra_idx < num_pim_ranks_; ra_idx++)
        {
                                  
            for (int gidx = 0; gidx < num_grfA_; gidx++)
            {
                string str = "WRIO_TO_GRF_";
                uint64_t addr =
                    pim_addr_mgr_->addrGen(ch_idx, ra_idx, 0, 1, pim_reg_ra_, 0x8 + gidx);
                int input_idx =
                    batchIdx * num_grfA_ * num_input_tiles + inputTile * num_grfA_ + gidx;
                                                                                        
                                                                              
                ++gemv_reuse_.generated_grfa_loads;
                if (gemv_reuse_.seen_loads.insert(
                        {ch_idx, ra_idx, batchIdx, inputTile, gidx}).second)
                    ++gemv_reuse_.distinct_logical_inputs;
                else
                    ++gemv_reuse_.repeated_logical_inputs;
                ++gemv_tile_.grfa_write_transactions;
                mem_->addTransaction(true, addr, str, &data->bData[input_idx]);
            }
            ++gemv_tile_.grfa_barrier_calls;
            mem_->addBarrier(ch_idx);
        }
    }

    unsigned row = 0;
    unsigned col = (num_grfA_ * num_grfB_) * (inputTile / 2 + outputTile * num_input_tiles / 2);

    for (int c_idx = 0; c_idx < 64; c_idx += 8)
    {
        gemv_tile_.mac_read_transactions +=
            uint64_t(num_pim_chans_) * num_pim_ranks_ * num_grfA_;
        gemv_tile_.mac_barrier_calls += num_pim_chans_;
        addTransactionAll(false, 0, (int)pb_type, row, col + c_idx, "MAC_", &null_bst_, true,
                          num_grfA_);
    }
}

void PIMKernel::readResult(BurstType* resultBst, pimBankType pb_type, int output_dim,
                           uint64_t base_addr, unsigned starting_row, unsigned starting_col)
{
    int ch_idx = 0;
    int ra_idx = 0;
    int bg_idx = 0;
    int bank_idx = 0;
    int bank_offset = (int)pb_type % 2;
    uint64_t addr;

    for (int x = 0; x < output_dim; x += num_grf_)
    {
        unsigned row = starting_row;
        unsigned col = starting_col;

        for (int grf_idx = 0; grf_idx < num_grf_; grf_idx++)
        {
            addr = pim_addr_mgr_->addrGenSafe(ch_idx, ra_idx, bg_idx, bank_idx + bank_offset, row,
                                              col);
            mem_->addTransaction(false, base_addr + addr, "output", &resultBst[x + grf_idx]);
            col++;
        }
        changeBank(pb_type, ch_idx, ra_idx, bg_idx, bank_idx, starting_row, starting_col, row, col);
    }
}

void PIMKernel::executeEltwise(int dim, pimBankType pb_type, KernelType ktype, int input0_row,
                               int result_row, int input1_row)
{
    int num_tile = dim / (num_banks_ * num_pim_chans_ * num_pim_ranks_ * num_grf_);
    int num_jump_to_be_taken = num_tile - 1;
    vector<PIMCmd> pim_cmds = PIMCmdGen::getPIMCmds(ktype, num_jump_to_be_taken, 0, 0);

    setControl(&bst_hab_pim_, true, getToggleCond(pb_type), false, false);
    setControl(&bst_hab_, false, getToggleCond(pb_type), false, false);

    parkIn();
    changePIMMode(dramMode::SB, dramMode::HAB);
    programCrf(pim_cmds);
    changePIMMode(dramMode::HAB, dramMode::HAB_PIM);

    if (ktype == KernelType::ADD || ktype == KernelType::MUL)
        computeAddOrMul(num_tile, input0_row, result_row, input1_row);
    else if (ktype == KernelType::RELU)
        computeRelu(num_tile, input0_row, result_row);
      
                                        
                                                   
       

    changePIMMode(dramMode::HAB_PIM, dramMode::HAB);
    changePIMMode(dramMode::HAB, dramMode::SB);
    parkOut();
}

void PIMKernel::computeAddOrMul(int num_tile, int input0_row, int result_row, int input1_row)
{
    for (int i = 0; i < num_tile; i++)
    {
        int c = num_grf_ * i;
        for (int b = 0; b < 2; b++)                                     
        {
            addTransactionAll(false, 0, b, input0_row, c, "BANK_TO_GRF_", &null_bst_, true,
                              num_grf_);
            addTransactionAll(false, 0, b, input1_row, c, "ADD", &null_bst_, true, num_grf_);
            addTransactionAll(true, 0, b, result_row, c, "GRF_TO_BANK", &null_bst_, true, num_grf_);
        }
    }
}

  
                                                                       
 
                                                           
     
                                                               
         
                                                                                                 
                                                                                                
                                                                     
         
     
                 

                                 
                                    
                                      
     
                                                                       
         
                                                                                        
                                              
                                                                                        
                                              
                                                                                               
                                              
         
     
 
  

void PIMKernel::computeRelu(int num_tile, int input0_row, int result_row)
{
    for (int i = 0; i < num_tile; i++)
    {
        int c = num_grf_ * i;
        addTransactionAll(false, 0, 0, input0_row, c, "FILL&ReLU", &null_bst_, true, num_grf_);
        addTransactionAll(true, 0, 0, result_row, c, "GRF_A_TO_EVEN_BANK", &null_bst_, true,
                          num_grf_);
        addTransactionAll(false, 0, 1, input0_row, c, "FILL&ReLU", &null_bst_, true, num_grf_);
        addTransactionAll(true, 0, 1, result_row, c, "GRF_B_TO_ODD_BANK", &null_bst_, true,
                          num_grf_);
    }
}

void PIMKernel::readData(BurstType* bst_data, size_t bst_cnt, unsigned starting_row,
                         unsigned starting_col)
{
    uint64_t init_addr = pim_addr_mgr_->addrGenSafe(0, 0, 0, 0, starting_row, starting_col);

    for (uint64_t addr = init_addr, i = 0; i < bst_cnt; addr += transaction_size_, i++)
    {
        mem_->addTransaction(false, addr, &bst_data[i]);
    }
}

void PIMKernel::adderTree(BurstType* result, int output_dim, int num_tile, int step, fp16* temp)
{
    if (num_tile == 1)
        return;

    int iter = num_tile / 2;
    if (step == 0)
    {
        for (int i = 0; i < iter; i++)
        {
            temp[i] = result[2 * i * output_dim].fp16AdderTree() +
                      result[(2 * i + 1) * output_dim].fp16AdderTree();
        }
    }
    else
    {
        for (int i = 0; i < iter; i++) temp[i] = temp[i * 2] + temp[i * 2 + 1];

        if (num_tile % 2 == 1)
            temp[iter] = temp[num_tile];
    }

    adderTree(result, output_dim, ceil(double(num_tile) / (double)2), step + 1, temp);

    return;
}
