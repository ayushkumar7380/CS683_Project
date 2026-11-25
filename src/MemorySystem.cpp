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

#include "MemorySystem.h"

#include <unistd.h>

using namespace std;

ofstream cmd_verify_out;                                                 
                                                       
namespace DRAMSim
{
powerCallBack_t MemorySystem::ReportPower = NULL;

MemorySystem::MemorySystem(unsigned id, unsigned int megsOfMemory, CSVWriter& csvOut_,
                           ostream& simLog, Configuration& configuration)
    : dramsimLog(simLog),
      ReturnReadData(NULL),
      WriteDataDone(NULL),
      systemID(id),
      csvOut(csvOut_),
      numOnTheFlyTransactions(0),
      config(configuration)
{
    currentClockCycle = 0;

    DEBUG("===== MemorySystem " << systemID << " =====");

                                                                                           

                                  
                             
                                                                                                  
                        
                                                                          
                                                                             
                                                                    
                                                                             
                                                                                
                                                                               
                                                                             
                                                                            
                                                                              
                                       
                          
                                                                              
                                                                                        
                                                                                   
                                                                                     
                                                  
                                                                                  
                                                    

                                                                                   
                                                                        
                                                                                                  
                                                                              
                                                                
                                                                             
                                                                               
                                                                                     
                                            
                          

                               
                                                                                           
                   
                                                                                                    

    uint64_t bytePerRank = static_cast<uint64_t>(
        (getConfigParam(UINT64, "NUM_ROWS") *
         (getConfigParam(UINT, "NUM_COLS") * getConfigParam(UINT, "DEVICE_WIDTH")) *
         getConfigParam(UINT, "NUM_BANKS") *
         (getConfigParam(UINT64, "JEDEC_DATA_BUS_BITS") / getConfigParam(UINT, "DEVICE_WIDTH"))) /
        8);
    uint64_t megsOfStoragePerRank = bytePerRank >> 20;

    num_ranks_ = (megsOfMemory / Byte2MB(bytePerRank));

                                                               
    if (megsOfMemory != 0)
    {
        num_ranks_ = (num_ranks_ > 0) ? num_ranks_ : 1;
        if (num_ranks_ == 0)
        {
            PRINT("WARNING: Cannot create memory system with "
                  << megsOfMemory << "MB, defaulting to minimum size of " << megsOfStoragePerRank
                  << "MB");
        }
        setDevConfigParam(UINT, "NUM_RANKS", num_ranks_);
                                                                               
        config.NUM_RANKS = num_ranks_;
    }

    unsigned num_devices =
        (getConfigParam(UINT, "JEDEC_DATA_BUS_BITS") / getConfigParam(UINT, "DEVICE_WIDTH"));
    setDevConfigParam(UINT, "NUM_DEVICES", num_devices);
    setDevConfigParam(UINT64, "TOTAL_STORAGE", num_ranks_ * Byte2MB(bytePerRank));

    PRINT("CH. " << systemID << " TOTAL_STORAGE : " << getConfigParam(UINT64, "TOTAL_STORAGE")
                 << "MB | " << getConfigParam(UINT, "NUM_RANKS") << " Ranks | "
                 << getConfigParam(UINT, "NUM_DEVICES") << " Devices per rank");

    memoryController = new MemoryController(this, csvOut, dramsimLog, config);

                                                
    ranks = new vector<Rank*>();

    for (size_t i = 0; i < num_ranks_; i++)
    {
        Rank* r = new Rank(dramsimLog, config);
        r->setChanId(systemID);
        r->setRankId(i);
        r->attachMemoryController(memoryController);
        r->pimRank->setChanId(systemID);
        r->pimRank->setRankId(i);
        ranks->push_back(r);
    }

    memoryController->attachRanks(ranks);
}

MemorySystem::~MemorySystem()
{
                                                                               
         
       
                                                               
                  

    delete (memoryController);

    for (size_t i = 0; i < num_ranks_; i++)
    {
        delete (*ranks)[i];
    }
    ranks->clear();
    delete (ranks);

    if (VERIFICATION_OUTPUT)
    {
        cmd_verify_out.flush();
        cmd_verify_out.close();
    }
}

bool MemorySystem::addTransaction(bool isWrite, uint64_t addr, BurstType* data)
{
    TransactionType type = isWrite ? DATA_WRITE : DATA_READ;
    Transaction* trans = new Transaction(type, addr, data);
    return addTransaction(trans);
}

bool MemorySystem::addTransaction(bool isWrite, uint64_t addr, const std::string& str,
                                  BurstType* data)
{
    TransactionType type = isWrite ? DATA_WRITE : DATA_READ;
    Transaction* trans = new Transaction(type, addr, str, data);
    return addTransaction(trans);
}

bool MemorySystem::addBarrier()
{
    if (memoryController->WillAcceptTransaction())
    {
        return memoryController->addBarrier();
    }
    else
    {
        if (pendingTransactions.size())
        {
            pendingTransactions.back()->tag += "BAR";
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool MemorySystem::addTransaction(Transaction* trans)
{
    if (memoryController->WillAcceptTransaction())
    {
        return memoryController->addTransaction(trans);
    }
    else
    {
        pendingTransactions.push_back(trans);
        return true;
    }
}

                    
void MemorySystem::printStats(bool finalStats)
{
    memoryController->printStats(finalStats);
}

                                  
void MemorySystem::update()
{
                                                                           

                                               
                                 
    for (size_t i = 0; i < num_ranks_; i++)
    {
        (*ranks)[i]->update();
    }

                                                                              
    if (pendingTransactions.size() > 0 && memoryController->WillAcceptTransaction())
    {
        memoryController->addTransaction(pendingTransactions.front());
        pendingTransactions.pop_front();
    }
    memoryController->update();

                                                                    
    for (size_t i = 0; i < num_ranks_; i++)
    {
        (*ranks)[i]->step();
    }
    memoryController->step();
    this->step();

                                    
}

void MemorySystem::RegisterCallbacks(Callback_t* readCB, Callback_t* writeCB,
                                     void (*reportPower)(double bgpower, double burstpower,
                                                         double refreshpower, double actprepower))
{
    ReturnReadData = readCB;
    WriteDataDone = writeCB;
    ReportPower = reportPower;
}

bool MemorySystem::WillAcceptTransaction(uint64_t addr)
{
    return memoryController->WillAcceptTransaction();
}

bool MemorySystem::WillAcceptTransaction()
{
    return memoryController->WillAcceptTransaction();
}

}                      

                                                           
                                            
                                              
extern "C" { void libdramsim_is_present(void) { ; } }
