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

#include <errno.h>
#include <stdlib.h>             

#include <bitset>
#include <iomanip>
#include <locale>
#include <sstream>                 
                           
#include <sys/stat.h>
#include <sys/types.h>

#include "AddressMapping.h"
#include "MultiChannelMemorySystem.h"
#include "ParameterReader.h"
#include "SystemConfiguration.h"
#include "Utils.h"

template <class T>
std::string FormatWithCommas(T value)
{
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << value;
    return ss.str();
}

using namespace DRAMSim;

MultiChannelMemorySystem::MultiChannelMemorySystem(const string& deviceIniFilename_,
                                                   const string& systemIniFilename_,
                                                   const string& pwd_, const string& traceFilename_,
                                                   unsigned megsOfMemory_, string* visFilename_)
    : megsOfMemory(megsOfMemory_),
      deviceIniFilename(deviceIniFilename_),
      systemIniFilename(systemIniFilename_),
      traceFilename(traceFilename_),
      pwd(pwd_),
      visFilename(visFilename_),
      clockDomainCrosser(new ClockDomain::Callback<MultiChannelMemorySystem, void>(
          this, &MultiChannelMemorySystem::actual_update)),
      csvOut(new CSVWriter(visDataOut))
{
    currentClockCycle = 0;
    if (visFilename)
        printf("CC VISFILENAME=%s\n", visFilename->c_str());

    if (!isPowerOfTwo(megsOfMemory))
    {
        ERROR("Please specify a power of 2 memory size");
        abort();
    }

    ConfigurationDB& configDB = ConfigurationDB::getDB();
    configDB.initialize();
    configDB.updatefromFile(deviceIniFilename);
    configDB.updatefromFile(systemIniFilename);

    addrMapping = new AddrMapping();
    configuration = new Configuration(*addrMapping);
    numFence = new unsigned[configuration->NUM_CHANS]();

    for (size_t i = 0; i < configuration->NUM_CHANS; i++)
    {
        MemorySystem* channel = new MemorySystem(i, megsOfMemory / configuration->NUM_CHANS,
                                                 (*csvOut), dramsimLog, *configuration);
        channels.push_back(channel);
    }
}

                                                         
                                                                         
   
void MultiChannelMemorySystem::setCPUClockSpeed(uint64_t cpuClkFreqHz)
{
    uint64_t dramsimClkFreqHz = (uint64_t)(1.0 / (configuration->tCK * 1e-9));
    clockDomainCrosser.clock1 = dramsimClkFreqHz;
    clockDomainCrosser.clock2 = (cpuClkFreqHz == 0) ? dramsimClkFreqHz : cpuClkFreqHz;
}

bool fileExists(string& path)
{
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0)
    {
        if (errno == ENOENT)
        {
            return false;
        }
        ERROR("Warning: some other kind of error happened with stat(), should probably check that");
    }
    return true;
}

string FilenameWithNumberSuffix(const string& filename, const string& extension,
                                unsigned maxNumber = 100)
{
    string currentFilename = filename + extension;
    if (!fileExists(currentFilename))
    {
        return currentFilename;
    }

                                                                                 
    stringstream tmpNum;
    tmpNum << "." << 1;
    for (unsigned i = 1; i < maxNumber; i++)
    {
        currentFilename = filename + tmpNum.str() + extension;
        if (fileExists(currentFilename))
        {
            currentFilename = filename;
            tmpNum.seekp(0);
            tmpNum << "." << i;
        }
        else
        {
            return currentFilename;
        }
    }
                                                                                     
    ERROR("Warning: Couldn't find a suitable suffix for " << filename);
    return currentFilename;
}
   
                                              
                                           
                                                            
                                                        
                                                                                                
                                                                                 
  
                                                                            
                             
    
void MultiChannelMemorySystem::InitOutputFiles(string traceFilename)
{
    size_t lastSlash;
    size_t deviceIniFilenameLength = deviceIniFilename.length();
    string sim_description_str;
    string deviceName;

    char* sim_description = getenv("SIM_DESC");
    if (sim_description)
    {
        sim_description_str = string(sim_description);
    }

                                                                               
                                     
    if (VERIFICATION_OUTPUT)
    {
        string basefilename = deviceIniFilename.substr(deviceIniFilename.find_last_of("/") + 1);
        string verify_filename = "sim_out_" + basefilename;
        if (sim_description != NULL)
        {
            verify_filename += "." + sim_description_str;
        }
        verify_filename += ".tmp";
        cmd_verify_out.open(verify_filename.c_str());
        if (!cmd_verify_out)
        {
            ERROR("Cannot open " << verify_filename);
            abort();
        }
    }
                                                                                    
                                    
    if (VIS_FILE_OUTPUT)
    {
        stringstream out, tmpNum;
        string path;
        string filename;

        if (!visFilename)
        {
            path = "results/";
                                              
            if (deviceIniFilename.substr(deviceIniFilenameLength - 4) == ".ini")
            {
                deviceName = deviceIniFilename.substr(0, deviceIniFilenameLength - 4);
                deviceIniFilenameLength -= 4;
            }

                                                                             
            if ((lastSlash = deviceName.find_last_of("/")) != string::npos)
            {
                deviceName =
                    deviceName.substr(lastSlash + 1, deviceIniFilenameLength - lastSlash - 1);
            }

            string rest;
                                                                          
            if ((lastSlash = traceFilename.find_last_of("/")) != string::npos)
            {
                traceFilename =
                    traceFilename.substr(lastSlash + 1, traceFilename.length() - lastSlash - 1);
            }
            if (sim_description != NULL)
            {
                traceFilename += "." + sim_description_str;
            }

            if (pwd.length() > 0)
            {
                path = pwd + "/" + path;
            }

                                                         
            mkdirIfNotExist(path);
            path = path + traceFilename + "/";
            mkdirIfNotExist(path);
            path = path + deviceName + "/";
            mkdirIfNotExist(path);

                                               
            string sched = "BtR";
            string queue = "pRank";
            if (configuration->SCHEDULING_POLICY == RankThenBankRoundRobin)
            {
                sched = "RtB";
            }
            if (configuration->QUEUING_STRUCTURE == PerRankPerBank)
            {
                queue = "pRankpBank";
            }

                                                                                 
                                                                                               
                                            
            out << (getConfigParam(UINT64, "TOTAL_STORAGE") >> 10) << "GB."
                << getConfigParam(UINT, "NUM_CHANS") << "Ch." << getConfigParam(UINT, "NUM_RANKS")
                << "R." << getConfigParam(UINT, "ADDRESS_MAPPING_SCHEME") << "."
                << getConfigParam(UINT, "ROW_BUFFER_POLICY") << "."
                << getConfigParam(UINT, "TRANS_QUEUE_DEPTH") << "TQ."
                << getConfigParam(UINT, "CMD_QUEUE_DEPTH") << "CQ." << sched << "." << queue;
        }
        else                      
        {
            out << *visFilename;
        }
        if (sim_description)
        {
            out << "." << sim_description;
        }

                                                                       
        filename = out.str();

        filename = FilenameWithNumberSuffix(filename, ".vis");
        path.append(filename);
        cerr << "writing vis file to " << path << endl;

        visDataOut.open(path.c_str());
        if (!visDataOut)
        {
            ERROR("Cannot open '" << path << "'");
            exit(-1);
        }
                                                                  
        ConfigurationDB& configDB = ConfigurationDB::getDB();
        configDB.dump(visDataOut);
    }
    else
    {
                                                
    }
    if (LOG_OUTPUT)
    {
                                                
        string dramsimLogFilename(SIM_TRACE_FILE);

        if (sim_description != NULL)
        {
            dramsimLogFilename += "." + sim_description_str;
        }

                                                                                        

        dramsimLog.open(dramsimLogFilename.c_str(), ios_base::out | ios_base::trunc);

        if (!dramsimLog)
        {
            ERROR("Cannot open " << dramsimLogFilename);
                           
        }
    }
}

void MultiChannelMemorySystem::mkdirIfNotExist(string path)
{
    struct stat stat_buf;
                                    
    if (stat(path.c_str(), &stat_buf) != 0)                                               
    {
        if (errno == ENOENT)
        {
                                                                         
                                                                   
            mode_t mode = (S_IXOTH | S_IXGRP | S_IXUSR | S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR);
            if (mkdir(path.c_str(), mode) != 0)
            {
                perror("Error Has occurred while trying to make directory: ");
                cerr << path << endl;
                abort();
            }
        }
        else
        {
            perror("Something else when wrong: ");
            abort();
        }
    }
    else                             
    {
        if (!S_ISDIR(stat_buf.st_mode))
        {
            ERROR(path << "is not a directory");
            abort();
        }
    }
}

MultiChannelMemorySystem::~MultiChannelMemorySystem()
{
                                 
    delete csvOut;
    delete[] numFence;
    delete addrMapping;
    delete configuration;

    for (size_t i = 0; i < configuration->NUM_CHANS; i++)
    {
        delete channels[i];
    }
    channels.clear();

                                          
    if (LOG_OUTPUT)
    {
        dramsimLog.flush();
        dramsimLog.close();
    }

    if (VIS_FILE_OUTPUT)
    {
        visDataOut.flush();
        visDataOut.close();
    }
}

void MultiChannelMemorySystem::update()
{
    clockDomainCrosser.update();
}

void MultiChannelMemorySystem::actual_update()
{
    if (currentClockCycle == 0)
    {
        InitOutputFiles(traceFilename);
        DEBUG("DRAMSim2 Clock Frequency =" << clockDomainCrosser.clock1
                                           << "Hz, CPU Clock Frequency="
                                           << clockDomainCrosser.clock2 << "Hz");
    }

    if (currentClockCycle > 0 && currentClockCycle % configuration->EPOCH_LENGTH == 0)
    {
        (*csvOut) << "ms" << currentClockCycle * configuration->tCK * 1E-6;
        for (size_t i = 0; i < configuration->NUM_CHANS; i++)
        {
            channels[i]->printStats(false);
        }
        csvOut->finalize();
    }

    for (size_t i = 0; i < configuration->NUM_CHANS; i++)
    {
        channels[i]->update();
    }

    currentClockCycle++;
}

unsigned MultiChannelMemorySystem::findChannelNumber(uint64_t addr)
{
                                                     
    if (configuration->NUM_CHANS == 1)
    {
        return 0;
    }

    if (!isPowerOfTwo(configuration->NUM_CHANS))
    {
        ERROR("We can only support power of two # of channels.\n"
              << "I don't know what Intel was thinking, but trying to address map half"
                 " a bit is a neat trick that we're not sure how to do");
        abort();
    }

                                      
    unsigned channelNumber, rank, bank, row, col;
    addrMapping->addressMapping(addr, channelNumber, rank, bank, row, col);
    if (channelNumber >= configuration->NUM_CHANS)
    {
        ERROR("Got channel index " << channelNumber << " but only " << configuration->NUM_CHANS
                                   << " exist");
        abort();
    }
    return channelNumber;
}

ostream& MultiChannelMemorySystem::getLogFile()
{
    return dramsimLog;
}

bool MultiChannelMemorySystem::addTransaction(Transaction* trans)
{
    unsigned channelNumber = findChannelNumber(trans->address);
    return channels[channelNumber]->addTransaction(trans);
}

bool MultiChannelMemorySystem::addBarrier(int chanId)
{
    numFence[chanId]++;
    return channels[chanId]->addBarrier();
}

bool MultiChannelMemorySystem::addTransaction(bool isWrite, uint64_t addr, BurstType* data)
{
    unsigned channelNumber = findChannelNumber(addr);
    return channels[channelNumber]->addTransaction(isWrite, addr, data);
}

bool MultiChannelMemorySystem::addTransaction(bool isWrite, uint64_t addr, const std::string& tag,
                                              BurstType* data)
{
    unsigned channelNumber = findChannelNumber(addr);
    return channels[channelNumber]->addTransaction(isWrite, addr, tag, data);
}

void MultiChannelMemorySystem::printStats(bool finalStats)
{
    if (finalStats)
    {
        CommandQueue::WaitFlags all_waits;
        CommandQueue::CounterfactualFlags all_cf;
        CommandQueue::TimingFlags all_timing;
        MemoryController::RowBankProfile row_stats;
        CommandQueue::SelectionOpportunity selection_stats;
        MemoryController::CommandStreamProfile stream_stats;
        MemoryController::PIMPhaseProfile phase_stats;
        uint64_t bank_min_columns = UINT64_MAX, bank_max_columns = 0;
        uint64_t bank_count = 0, banks_with_columns = 0;
        uint64_t controller_cycles = 0, queued_no_issue = 0, issued_read = 0,
                 issued_write = 0, issued_activate = 0, issued_precharge = 0,
                 issued_refresh = 0, pim_read = 0, pim_write = 0;
        uint64_t pim_mode_cycles = 0, mac_issue_cycles = 0,
                 mac_instructions = 0, bank_operand_reads = 0,
                 nonmac_instructions = 0;
        for (auto* ch : channels)
        {
            const auto ph = ch->memoryController->getPIMPhaseProfile();
            phase_stats.mac_runs_completed += ph.mac_runs_completed;
            phase_stats.mac_run_length_sum += ph.mac_run_length_sum;
            phase_stats.mac_gap_events += ph.mac_gap_events;
            phase_stats.mac_gap_commands += ph.mac_gap_commands;
            phase_stats.mac_gap_cycles += ph.mac_gap_cycles;
            phase_stats.mac_break_grf += ph.mac_break_grf;
            phase_stats.mac_break_barrier += ph.mac_break_barrier;
            phase_stats.mac_break_crf += ph.mac_break_crf;
            phase_stats.mac_break_mode += ph.mac_break_mode;
            phase_stats.mac_break_actpre += ph.mac_break_actpre;
            phase_stats.mac_break_other += ph.mac_break_other;
            phase_stats.gap_grf += ph.gap_grf;
            phase_stats.gap_barrier += ph.gap_barrier;
            phase_stats.gap_crf += ph.gap_crf;
            phase_stats.gap_mode += ph.gap_mode;
            phase_stats.gap_actpre += ph.gap_actpre;
            phase_stats.gap_other += ph.gap_other;
            phase_stats.grf_before_mac_run += ph.grf_before_mac_run;
            phase_stats.barrier_before_mac_run += ph.barrier_before_mac_run;
            phase_stats.mac_run_bank_switches += ph.mac_run_bank_switches;
            phase_stats.phase_setup_commands += ph.phase_setup_commands;
            phase_stats.phase_compute_commands += ph.phase_compute_commands;
            phase_stats.phase_teardown_commands += ph.phase_teardown_commands;
            phase_stats.phase_first_to_last_mac_cycles += ph.phase_first_to_last_mac_cycles;
            phase_stats.phase_first_to_last_mac_valid += ph.phase_first_to_last_mac_valid;
            phase_stats.barrier_tagged_grf += ph.barrier_tagged_grf;
            phase_stats.barrier_tagged_mac += ph.barrier_tagged_mac;
            phase_stats.barrier_tagged_other += ph.barrier_tagged_other;
            phase_stats.grf_load_tagged += ph.grf_load_tagged;
            phase_stats.grf_store_tagged += ph.grf_store_tagged;
            if (ph.max_mac_gap_commands > phase_stats.max_mac_gap_commands)
                phase_stats.max_mac_gap_commands = ph.max_mac_gap_commands;
            if (ph.max_mac_gap_cycles > phase_stats.max_mac_gap_cycles)
                phase_stats.max_mac_gap_cycles = ph.max_mac_gap_cycles;
            const auto st = ch->memoryController->getCommandStreamProfile();
            stream_stats.issued += st.issued;
            stream_stats.tagged += st.tagged;
            stream_stats.barrier_tagged += st.barrier_tagged;
            stream_stats.barrier_tagged_columns += st.barrier_tagged_columns;
            stream_stats.mac_tagged += st.mac_tagged;
            stream_stats.mac_pim_mode += st.mac_pim_mode;
            stream_stats.crf_tagged += st.crf_tagged;
            stream_stats.grf_tagged += st.grf_tagged;
            stream_stats.mode_tagged += st.mode_tagged;
            stream_stats.other_tagged += st.other_tagged;
            stream_stats.pim_mode_column_commands += st.pim_mode_column_commands;
            stream_stats.consecutive_same_bank_columns += st.consecutive_same_bank_columns;
            stream_stats.consecutive_other_bank_columns += st.consecutive_other_bank_columns;
            stream_stats.consecutive_same_rank_columns += st.consecutive_same_rank_columns;
            stream_stats.column_streams += st.column_streams;
            stream_stats.mac_runs += st.mac_runs;
            if (st.max_mac_run > stream_stats.max_mac_run)
                stream_stats.max_mac_run = st.max_mac_run;
            auto c = ch->memoryController->getBaselineCounters();
            controller_cycles += c.cycles;
            selection_stats.selected_column += c.selection_opportunity.selected_column;
            selection_stats.selected_with_alternative += c.selection_opportunity.selected_with_alternative;
            selection_stats.selected_with_other_bank += c.selection_opportunity.selected_with_other_bank;
            selection_stats.selected_with_same_bank += c.selection_opportunity.selected_with_same_bank;
            selection_stats.selected_with_better_lookahead += c.selection_opportunity.selected_with_better_lookahead;
            selection_stats.selected_with_equal_lookahead += c.selection_opportunity.selected_with_equal_lookahead;
            selection_stats.selected_with_worse_lookahead += c.selection_opportunity.selected_with_worse_lookahead;
            selection_stats.selected_pim_mode += c.selection_opportunity.selected_pim_mode;
            selection_stats.pim_with_alternative += c.selection_opportunity.pim_with_alternative;
            selection_stats.pim_with_other_bank += c.selection_opportunity.pim_with_other_bank;
            selection_stats.pim_with_better_lookahead += c.selection_opportunity.pim_with_better_lookahead;
            selection_stats.candidate_alternatives_total += c.selection_opportunity.candidate_alternatives_total;
            selection_stats.candidate_other_bank_total += c.selection_opportunity.candidate_other_bank_total;

            row_stats.enqueued_column_requests += c.row_bank.enqueued_column_requests;
            row_stats.enqueue_row_open_hit += c.row_bank.enqueue_row_open_hit;
            row_stats.enqueue_wrong_open_row += c.row_bank.enqueue_wrong_open_row;
            row_stats.enqueue_row_not_open += c.row_bank.enqueue_row_not_open;
            row_stats.column_reads += c.row_bank.column_reads;
            row_stats.column_writes += c.row_bank.column_writes;
            row_stats.pim_column_reads += c.row_bank.pim_column_reads;
            row_stats.pim_column_writes += c.row_bank.pim_column_writes;
            row_stats.activate += c.row_bank.activate;
            row_stats.precharge += c.row_bank.precharge;
            row_stats.activate_same_row += c.row_bank.activate_same_row;
            row_stats.activate_different_row += c.row_bank.activate_different_row;
            row_stats.row_hit_on_issue += c.row_bank.row_hit_on_issue;
            row_stats.row_not_open_on_issue += c.row_bank.row_not_open_on_issue;
            row_stats.wrong_row_on_issue += c.row_bank.wrong_row_on_issue;
            row_stats.pim_row_hit_on_issue += c.row_bank.pim_row_hit_on_issue;
            row_stats.active_bank_samples += c.row_bank.active_bank_samples;
            row_stats.active_bank_sample_sum += c.row_bank.active_bank_sample_sum;
            if (c.row_bank.active_bank_sample_max > row_stats.active_bank_sample_max)
                row_stats.active_bank_sample_max = c.row_bank.active_bank_sample_max;
            for (const auto& bank : ch->memoryController->getPerBankRowProfiles()) {
                const uint64_t columns = bank.column_reads + bank.column_writes;
                ++bank_count;
                if (columns) ++banks_with_columns;
                if (columns < bank_min_columns) bank_min_columns = columns;
                if (columns > bank_max_columns) bank_max_columns = columns;
            }
            queued_no_issue += c.queued_no_issue;
            all_timing.wait_tRCD += c.timing.wait_tRCD;
            all_timing.wait_tCCD_or_burst += c.timing.wait_tCCD_or_burst;
            all_timing.wait_bus_turnaround += c.timing.wait_bus_turnaround;
            all_timing.wait_rank_turnaround += c.timing.wait_rank_turnaround;
            all_timing.wait_tRP += c.timing.wait_tRP;
            all_timing.wait_tRAS += c.timing.wait_tRAS;
            all_timing.wait_tRRD += c.timing.wait_tRRD;
            all_timing.wait_tRC += c.timing.wait_tRC;
            all_timing.wait_tRFC += c.timing.wait_tRFC;
            all_timing.wait_tFAW += c.timing.wait_tFAW;
            all_timing.wait_read_to_pre += c.timing.wait_read_to_pre;
            all_timing.wait_write_to_pre += c.timing.wait_write_to_pre;
            all_timing.wait_row_not_open += c.timing.wait_row_not_open;
            all_timing.wait_wrong_row += c.timing.wait_wrong_row;
            all_timing.wait_bank_state += c.timing.wait_bank_state;
            all_timing.wait_pim_bank_restricted += c.timing.wait_pim_bank_restricted;
            all_timing.wait_row_access_limit += c.timing.wait_row_access_limit;
            all_timing.wait_unknown_gate += c.timing.wait_unknown_gate;
            all_cf.barrier_present += c.counterfactual.barrier_present;
            all_cf.barrier_blocks_ready_command += c.counterfactual.barrier_blocks_ready_command;
            all_cf.dependency_blocks_ready_command += c.counterfactual.dependency_blocks_ready_command;
            all_cf.timing_only_blocked += c.counterfactual.timing_only_blocked;
            all_cf.issuable_but_not_selected += c.counterfactual.issuable_but_not_selected;
            all_cf.ready_behind_barrier_with_dependency += c.counterfactual.ready_behind_barrier_with_dependency;
            all_cf.any_dram_ready_queued_command += c.counterfactual.any_dram_ready_queued_command;
            all_cf.head_dram_ready += c.counterfactual.head_dram_ready;
            all_waits.barrier += c.waits.barrier;
            all_waits.address_dependency += c.waits.address_dependency;
            all_waits.read_write_timing += c.waits.read_write_timing;
            all_waits.needs_activate += c.waits.needs_activate;
            all_waits.wrong_open_row += c.waits.wrong_open_row;
            all_waits.row_limit += c.waits.row_limit;
            all_waits.activate_timing += c.waits.activate_timing;
            all_waits.pim_bank_restricted += c.waits.pim_bank_restricted;
            all_waits.precharge_timing += c.waits.precharge_timing;
            all_waits.other += c.waits.other;
            issued_read += c.read;
            issued_write += c.write;
            issued_activate += c.activate;
            issued_precharge += c.precharge;
            issued_refresh += c.refresh;
            pim_read += c.pim_read;
            pim_write += c.pim_write;
            for (auto* rank : *ch->ranks)
            {
                const auto& r = rank->pimRank->baseline;
                pim_mode_cycles += r.pim_mode_cycles;
                mac_issue_cycles += r.mac_issue_cycles;
                mac_instructions += r.mac_instructions;
                bank_operand_reads += r.bank_operand_reads;
                nonmac_instructions += r.nonmac_pim_instructions;
            }
        }
                                                                            
        uint64_t grfa_writes=0, grfa_same=0, grfa_changed=0, grfa_unread=0;
        uint64_t grfa_mac_reads=0, grfa_before=0, grfa_after=0, grfa_reused=0;
        uint64_t grfa_zero=0, grfa_live_unread=0;
        for (const auto& ch : channels)
            for (auto* rank : *ch->ranks)
            {
                const auto& v = rank->pimRank->grfa_reuse;
                grfa_writes += v.grfa_broadcast_writes;
                grfa_same += v.grfa_same_value_overwrites;
                grfa_changed += v.grfa_changed_value_overwrites;
                grfa_unread += v.grfa_overwrites_before_read;
                grfa_mac_reads += v.grfa_mac_operand_reads;
                grfa_before += v.grfa_mac_reads_before_load;
                grfa_after += v.grfa_mac_reads_after_load;
                grfa_reused += v.grfa_mac_reuses_since_load;
                grfa_zero += v.grfa_zeroizations;
                for (unsigned i = 0; i < 8; ++i)
                    if (rank->pimRank->grfa_seen_write[i] &&
                        !rank->pimRank->grfa_read_since_write[i]) ++grfa_live_unread;
            }
        uint64_t step13_restore_issued = 0, step13_stall_cycles = 0;
        uint64_t step13_stall_with_queue = 0;
        unsigned step13_configured_penalty = 0;
        for (const auto& ch : channels) {
            const auto d = ch->memoryController->getStep13RestoreStats();
            step13_restore_issued += d.restore_issued;
            step13_stall_cycles += d.cooldown_cycles;
            step13_stall_with_queue += d.cooldown_with_queued_commands;
            step13_configured_penalty = d.extra_cycles;
        }
        std::cout << "[STEP13_RUNTIME] restore_issues=" << step13_restore_issued
                  << " channel_stall_cycles=" << step13_stall_cycles
                  << " channel_stall_with_queue=" << step13_stall_with_queue
                  << " penalty_per_restore=" << step13_configured_penalty
                  << " aggregation=sum_across_channels"
                  << " model=post_restore_command_issue_cooldown"
                  << " not_physical_sram_latency=1\n";
        uint64_t distinct_payload_hashes_sum = 0, zero_payload_writes = 0;
        for (const auto& ch : channels)
            for (auto* rank : *ch->ranks)
            {
                distinct_payload_hashes_sum += rank->pimRank->grfa_unique_payload_hashes.size();
                zero_payload_writes += rank->pimRank->grfa_zero_payload_writes;
            }
        std::cout << "[GRFA_VALUE_PROFILE]"
                  << " unique_payload_hashes_per_rank_sum=" << distinct_payload_hashes_sum
                  << " zero_payload_writes=" << zero_payload_writes
                  << " unique_hashes_are_per_rank_not_global=1\n";
        std::cout << "[GRF_A_RUNTIME]"
                  << " grfa_broadcast_writes=" << grfa_writes
                  << " same_value_overwrites=" << grfa_same
                  << " changed_value_overwrites=" << grfa_changed
                  << " overwrites_before_read=" << grfa_unread
                  << " mac_operand_reads=" << grfa_mac_reads
                  << " mac_reads_before_load=" << grfa_before
                  << " mac_reads_after_load=" << grfa_after
                  << " mac_reuses_since_load=" << grfa_reused
                  << " zeroizations=" << grfa_zero
                  << " live_unread_slots_at_end=" << grfa_live_unread
                  << "\n";
        std::cout << "\n[BASELINE_PROFILE] channels=" << channels.size()
                  << " controller_channel_cycles=" << controller_cycles
                  << " queued_no_issue_channel_cycles=" << queued_no_issue
                  << " issued_read=" << issued_read
                  << " issued_write=" << issued_write
                  << " issued_activate=" << issued_activate
                  << " issued_precharge=" << issued_precharge
                  << " issued_refresh=" << issued_refresh
                  << " issued_pim_mode_read=" << pim_read
                  << " issued_pim_mode_write=" << pim_write
                  << " pim_rank_mode_cycles=" << pim_mode_cycles
                  << " pim_rank_mac_issue_cycles=" << mac_issue_cycles
                  << " mac_instructions=" << mac_instructions
                  << " bank_operand_reads=" << bank_operand_reads
                  << " other_pim_instructions=" << nonmac_instructions
                  << "\n";
        std::cout << "[SCHED_DIAG] queued_no_issue_channel_cycles=" << queued_no_issue
                  << " barrier=" << all_waits.barrier
                  << " address_dependency=" << all_waits.address_dependency
                  << " rw_timing=" << all_waits.read_write_timing
                  << " needs_activate=" << all_waits.needs_activate
                  << " wrong_open_row=" << all_waits.wrong_open_row
                  << " row_access_limit=" << all_waits.row_limit
                  << " activate_timing=" << all_waits.activate_timing
                  << " pim_bank_restricted=" << all_waits.pim_bank_restricted
                  << " precharge_timing=" << all_waits.precharge_timing
                  << " other=" << all_waits.other << "\n";
        std::cout << "[SCHED_COUNTERFACTUAL] queued_no_issue_channel_cycles=" << queued_no_issue
                  << " barrier_present=" << all_cf.barrier_present
                  << " barrier_blocks_ready_command=" << all_cf.barrier_blocks_ready_command
                  << " dependency_blocks_ready_command=" << all_cf.dependency_blocks_ready_command
                  << " timing_only_blocked=" << all_cf.timing_only_blocked
                  << " issuable_but_not_selected=" << all_cf.issuable_but_not_selected
                  << " ready_behind_barrier_with_dependency=" << all_cf.ready_behind_barrier_with_dependency
                  << " any_dram_ready_queued_command=" << all_cf.any_dram_ready_queued_command
                  << " head_dram_ready=" << all_cf.head_dram_ready << "\n";
        std::cout << "[DRAM_TIMING_DIAG] queued_no_issue_channel_cycles=" << queued_no_issue
                  << " wait_tRCD=" << all_timing.wait_tRCD
                  << " wait_tCCD_or_burst=" << all_timing.wait_tCCD_or_burst
                  << " wait_bus_turnaround=" << all_timing.wait_bus_turnaround
                  << " wait_rank_turnaround=" << all_timing.wait_rank_turnaround
                  << " wait_tRP=" << all_timing.wait_tRP
                  << " wait_tRAS=" << all_timing.wait_tRAS
                  << " wait_tRRD=" << all_timing.wait_tRRD
                  << " wait_tRC=" << all_timing.wait_tRC
                  << " wait_tRFC=" << all_timing.wait_tRFC
                  << " wait_tFAW=" << all_timing.wait_tFAW
                  << " wait_read_to_pre=" << all_timing.wait_read_to_pre
                  << " wait_write_to_pre=" << all_timing.wait_write_to_pre
                  << " wait_row_not_open=" << all_timing.wait_row_not_open
                  << " wait_wrong_row=" << all_timing.wait_wrong_row
                  << " wait_bank_state=" << all_timing.wait_bank_state
                  << " wait_pim_bank_restricted=" << all_timing.wait_pim_bank_restricted
                  << " wait_row_access_limit=" << all_timing.wait_row_access_limit
                  << " wait_unknown_gate=" << all_timing.wait_unknown_gate
                  << "\n";
        std::cout << "[ROW_BANK_PROFILE] enqueued_requests=" << row_stats.enqueued_column_requests
                  << " enqueue_row_open_hit=" << row_stats.enqueue_row_open_hit
                  << " enqueue_wrong_open_row=" << row_stats.enqueue_wrong_open_row
                  << " enqueue_row_not_open=" << row_stats.enqueue_row_not_open
                  << " issued_column_reads=" << row_stats.column_reads
                  << " issued_column_writes=" << row_stats.column_writes
                  << " pim_column_reads=" << row_stats.pim_column_reads
                  << " pim_column_writes=" << row_stats.pim_column_writes
                  << " row_hits_on_issue=" << row_stats.row_hit_on_issue
                  << " row_not_open_on_issue=" << row_stats.row_not_open_on_issue
                  << " wrong_row_on_issue=" << row_stats.wrong_row_on_issue
                  << " pim_row_hits_on_issue=" << row_stats.pim_row_hit_on_issue
                  << " activates=" << row_stats.activate
                  << " precharges=" << row_stats.precharge
                  << " reactivates_same_row=" << row_stats.activate_same_row
                  << " activates_different_row=" << row_stats.activate_different_row
                  << " total_banks=" << bank_count
                  << " banks_with_columns=" << banks_with_columns
                  << " min_columns_per_bank=" << (bank_count ? bank_min_columns : 0)
                  << " max_columns_per_bank=" << bank_max_columns
                  << " active_bank_samples=" << row_stats.active_bank_samples
                  << " active_banks_at_issue_sum=" << row_stats.active_bank_sample_sum
                  << " active_banks_at_issue_max=" << row_stats.active_bank_sample_max
                  << "\n";
        std::cout << "[SCHED_OPPORTUNITY]"
                  << " selected_column=" << selection_stats.selected_column
                  << " selected_with_alternative=" << selection_stats.selected_with_alternative
                  << " selected_with_other_bank=" << selection_stats.selected_with_other_bank
                  << " selected_with_same_bank=" << selection_stats.selected_with_same_bank
                  << " selected_with_better_lookahead=" << selection_stats.selected_with_better_lookahead
                  << " selected_with_equal_lookahead=" << selection_stats.selected_with_equal_lookahead
                  << " selected_with_worse_lookahead=" << selection_stats.selected_with_worse_lookahead
                  << " selected_pim_mode=" << selection_stats.selected_pim_mode
                  << " pim_with_alternative=" << selection_stats.pim_with_alternative
                  << " pim_with_other_bank=" << selection_stats.pim_with_other_bank
                  << " pim_with_better_lookahead=" << selection_stats.pim_with_better_lookahead
                  << " candidate_alternatives_total=" << selection_stats.candidate_alternatives_total
                  << " candidate_other_bank_total=" << selection_stats.candidate_other_bank_total
                  << "\n";
        std::cout << "[PIM_STREAM_PROFILE]"
                  << " issued=" << stream_stats.issued
                  << " tagged=" << stream_stats.tagged
                  << " barrier_tagged=" << stream_stats.barrier_tagged
                  << " barrier_tagged_columns=" << stream_stats.barrier_tagged_columns
                  << " mac_tagged=" << stream_stats.mac_tagged
                  << " mac_pim_mode=" << stream_stats.mac_pim_mode
                  << " crf_tagged=" << stream_stats.crf_tagged
                  << " grf_tagged=" << stream_stats.grf_tagged
                  << " mode_tagged=" << stream_stats.mode_tagged
                  << " other_tagged=" << stream_stats.other_tagged
                  << " pim_mode_columns=" << stream_stats.pim_mode_column_commands
                  << " adjacent_same_bank_columns=" << stream_stats.consecutive_same_bank_columns
                  << " adjacent_other_bank_columns=" << stream_stats.consecutive_other_bank_columns
                  << " adjacent_same_rank_columns=" << stream_stats.consecutive_same_rank_columns
                  << " column_streams=" << stream_stats.column_streams
                  << " mac_runs=" << stream_stats.mac_runs
                  << " max_mac_run_per_channel=" << stream_stats.max_mac_run << "\n";
        std::cout << "[PIM_PHASE_PROFILE]"
                  << " mac_runs_completed=" << phase_stats.mac_runs_completed
                  << " mac_run_length_sum=" << phase_stats.mac_run_length_sum
                  << " mac_gap_events=" << phase_stats.mac_gap_events
                  << " mac_gap_commands=" << phase_stats.mac_gap_commands
                  << " mac_gap_cycles=" << phase_stats.mac_gap_cycles
                  << " max_mac_gap_commands=" << phase_stats.max_mac_gap_commands
                  << " max_mac_gap_cycles=" << phase_stats.max_mac_gap_cycles
                  << " mac_break_grf=" << phase_stats.mac_break_grf
                  << " mac_break_barrier=" << phase_stats.mac_break_barrier
                  << " mac_break_crf=" << phase_stats.mac_break_crf
                  << " mac_break_mode=" << phase_stats.mac_break_mode
                  << " mac_break_actpre=" << phase_stats.mac_break_actpre
                  << " mac_break_other=" << phase_stats.mac_break_other
                  << " gap_grf=" << phase_stats.gap_grf
                  << " gap_barrier=" << phase_stats.gap_barrier
                  << " gap_crf=" << phase_stats.gap_crf
                  << " gap_mode=" << phase_stats.gap_mode
                  << " gap_actpre=" << phase_stats.gap_actpre
                  << " gap_other=" << phase_stats.gap_other
                  << " grf_before_mac_run=" << phase_stats.grf_before_mac_run
                  << " barrier_before_mac_run=" << phase_stats.barrier_before_mac_run
                  << " mac_run_bank_switches=" << phase_stats.mac_run_bank_switches
                  << " phase_setup_commands=" << phase_stats.phase_setup_commands
                  << " phase_compute_commands=" << phase_stats.phase_compute_commands
                  << " phase_teardown_commands=" << phase_stats.phase_teardown_commands
                  << " phase_first_to_last_mac_cycles=" << phase_stats.phase_first_to_last_mac_cycles
                  << " phase_first_to_last_mac_valid=" << phase_stats.phase_first_to_last_mac_valid
                  << " barrier_tagged_grf=" << phase_stats.barrier_tagged_grf
                  << " barrier_tagged_mac=" << phase_stats.barrier_tagged_mac
                  << " barrier_tagged_other=" << phase_stats.barrier_tagged_other
                  << " grf_load_tagged=" << phase_stats.grf_load_tagged
                  << " grf_store_tagged=" << phase_stats.grf_store_tagged << "\n";
                                                                                                                                      
                                                                                  
                                                                           
                                                                           
                                                                               
                                                                         
                                                                        
    }

    uint64_t cyclesElapsed;
    MemoryController* mem_ctrl;
    unsigned IDD3N, IDD3NC, IDD3NQ;
    float Vpp, Vddc, Vddq;

    IDD3N = getConfigParam(UINT, "IDD3N");
    IDD3NC = getConfigParam(UINT, "IDD3NC");
    IDD3NQ = getConfigParam(UINT, "IDD3NQ");
    Vpp = getConfigParam(FLOAT, "Vpp");
    Vddc = getConfigParam(FLOAT, "Vddc");
    Vddq = getConfigParam(FLOAT, "Vddq");

    bool printEnergy = false;
    uint64_t total_burstEnergy = 0;
    uint64_t total_actpreEnergy = 0;
    uint64_t total_aluPIMEnergy = 0;
    uint64_t total_energy = 0;
    double total_burstPower = 0.0;
    double total_actprePower = 0.0;
    double total_aluPIMPower = 0.0;
    double total_power = 0.0;
    double total_bandwidth = 0.0;
    uint64_t total_num_mac = 0;
    uint64_t totalReads = 0;
    uint64_t totalWrites = 0;

    (*csvOut) << "ms" << currentClockCycle * configuration->tCK * 1E-6;
    for (size_t i = 0; i < configuration->NUM_CHANS; i++)
    {
        PRINTC(PRINT_CHAN_STAT, "==== Channel [" << i << "] ====");
        channels[i]->printStats(finalStats);
        PRINTC(PRINT_CHAN_STAT, "//// Channel [" << i << "] ////");
    }

    for (size_t i = 0; i < getConfigParam(UINT, "NUM_CHANS"); i++)
    {
        mem_ctrl = channels[i]->memoryController;
        cyclesElapsed = (mem_ctrl->currentClockCycle % configuration->EPOCH_LENGTH == 0)
                            ? configuration->EPOCH_LENGTH
                            : mem_ctrl->currentClockCycle % configuration->EPOCH_LENGTH;
        for (size_t r = 0; r < getConfigParam(UINT, "NUM_RANKS"); r++)
        {
            total_burstEnergy += mem_ctrl->burstEnergy[r];
            total_actpreEnergy += mem_ctrl->actpreEnergy[r];
            total_aluPIMEnergy += mem_ctrl->aluPIMEnergy[r];
            total_burstPower += ((double)mem_ctrl->burstEnergy[r] / cyclesElapsed / 1000.0);
            total_actprePower += ((double)mem_ctrl->actpreEnergy[r] / cyclesElapsed / 1000.0);
            total_aluPIMPower += ((double)mem_ctrl->aluPIMEnergy[r] / cyclesElapsed / 1000.0);
        }
        totalReads += mem_ctrl->totalReads;
        totalWrites += mem_ctrl->totalWrites;

        total_bandwidth += mem_ctrl->totalBandwidth;

        mem_ctrl->resetStats();
    }

    backgroundPower = (IDD3N * Vpp + IDD3NC * Vddc + IDD3NQ * Vddq) / 1000.0;
    total_energy = total_burstEnergy + total_actpreEnergy + total_aluPIMEnergy;
    total_power = backgroundPower + total_burstPower + total_actprePower + total_aluPIMPower;

    PRINT("//// Simulation Results ////");
    PRINT("        Total Simulated Cycle (assuming 1GHz)  : " << currentClockCycle);
    PRINT("        Total Simulated Time (s)  : " << currentClockCycle * configuration->tCK * 1E-9);
    if (printEnergy)
    {
        PRINT("        Background Power(watts)   : " << backgroundPower);

        PRINT("        Total Power(watts)   : " << total_power);
        if (total_energy * 1E-9 < 1)
        {
            PRINT("        Total Energy(uJ)     : " << total_energy * 1E-6);
        }
        else
        {
            PRINT("        Total Energy(mJ)     : " << total_energy * 1E-9);
        }
    }
    unsigned JEDEC_DATA_BUS_BITS = getConfigParam(UINT, "JEDEC_DATA_BUS_BITS");
    unsigned BL = getConfigParam(UINT, "BL");
    PRINT("        Total Bandwidth(GB/s): " << (totalReads + totalWrites) * JEDEC_DATA_BUS_BITS *
                                                   BL / 8 /
                                                   (currentClockCycle * configuration->tCK));
    if (total_num_mac > 0)
    {
        PRINT("        Total Mac            : " << FormatWithCommas<uint64_t>(total_num_mac));
        PRINT("        TOPS/s            : "
              << (total_num_mac * 2 / 1e12) / (currentClockCycle * configuration->tCK * 1E-9));
    }

    PRINT("");

    csvOut->finalize();
}

void MultiChannelMemorySystem::RegisterCallbacks(
    TransactionCompleteCB* readDone, TransactionCompleteCB* writeDone,
    void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower))
{
    for (size_t i = 0; i < configuration->NUM_CHANS; i++)
    {
        channels[i]->RegisterCallbacks(readDone, writeDone, reportPower);
    }
}

int MultiChannelMemorySystem::hasPendingTransactions()
{
    int num = 0;
    for (auto chan : channels)
    {
        num += chan->numOnTheFlyTransactions;
    }
    return num;
}

bool MultiChannelMemorySystem::willAcceptTransaction(uint64_t addr)
{
    unsigned chan, rank, bank, row, col;
    addrMapping->addressMapping(addr, chan, rank, bank, row, col);
    return channels[chan]->WillAcceptTransaction();
}

bool MultiChannelMemorySystem::willAcceptTransaction()
{
    for (size_t c = 0; c < configuration->NUM_CHANS; c++)
    {
        if (!channels[c]->WillAcceptTransaction())
        {
            return false;
        }
    }
    return true;
}
