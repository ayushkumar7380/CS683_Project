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

#include "AddressMapping.h"

#include <bitset>
#include <iostream>

#include "SystemConfiguration.h"
#include "Utils.h"

using namespace std;

namespace DRAMSim
{
AddrMapping::AddrMapping()
{
    transactionSize = getConfigParam(UINT, "JEDEC_DATA_BUS_BITS") / 8 * getConfigParam(UINT, "BL");
                                                                                    
    transactionMask = transactionSize - 1;
    channelBitWidth = uLog2(getConfigParam(UINT, "NUM_CHANS"));
    rankBitWidth = uLog2(getConfigParam(UINT, "NUM_RANKS"));
    bankBitWidth = uLog2(getConfigParam(UINT, "NUM_BANKS"));
    bankgroupBitWidth = uLog2(getConfigParam(UINT, "NUM_BANK_GROUPS"));
    rowBitWidth = uLog2(getConfigParam(UINT, "NUM_ROWS"));
    colBitWidth = uLog2(getConfigParam(UINT, "NUM_COLS"));
    byteOffsetWidth = uLog2(getConfigParam(UINT, "JEDEC_DATA_BUS_BITS") / 8);
    unsigned throw_away_bits = uLog2(transactionSize);
    colLowBitWidth = (throw_away_bits - byteOffsetWidth);
    colHighBitWidth = colBitWidth - colLowBitWidth;
    num_chans_ = getConfigParam(UINT, "NUM_CHANS");
    num_bank_per_bg_ = getConfigParam(UINT, "NUM_BANKS") / getConfigParam(UINT, "NUM_BANK_GROUPS");
    addressMappingScheme = PIMConfiguration::getAddressMappingScheme();
}

unsigned AddrMapping::bankgroupId(int bank)
{
                                                                        
    return (bank / num_bank_per_bg_);
}

bool AddrMapping::isSameBankgroup(int bank0, int bank1)
{
    return bankgroupId(bank0) == bankgroupId(bank1);
}

void AddrMapping::addressMapping(uint64_t physicalAddress, unsigned& newTransactionChan,
                                 unsigned& newTransactionRank, unsigned& newTransactionBank,
                                 unsigned& newTransactionRow, unsigned& newTransactionColumn)
{
    if ((physicalAddress & transactionMask) != 0)
    {
        DEBUG("WARNING: address 0x" << std::hex << physicalAddress << std::dec
                                    << " is not aligned to the request size of "
                                    << transactionSize);
    }
                                                                                 
                                                                                                  
    physicalAddress >>= byteOffsetWidth;

                                                                              
                                                                            
                                                                            
                                                                                
                                                 
      
                                                                             
                                                                              
                                                                            
                                                               
      
                                                                               
                                                                                
                                                                            
           
                                         
      
                                                                

    physicalAddress >>= colLowBitWidth;

    if (DEBUG_ADDR_MAP)
    {
        DEBUG("Bit widths: ch:" << channelBitWidth << " r:" << rankBitWidth << " b:" << bankBitWidth
                                << " row:" << rowBitWidth << " colLow:" << colLowBitWidth
                                << " colHigh:" << colHighBitWidth << " off:" << byteOffsetWidth
                                << " Total:"
                                << (channelBitWidth + rankBitWidth + bankBitWidth + rowBitWidth +
                                    colLowBitWidth + colHighBitWidth + byteOffsetWidth));
    }

                                              
    if (addressMappingScheme == Scheme1)
    {
                                 
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth);
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
    }
    else if (addressMappingScheme == Scheme2)
    {
                                 
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth);
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
    }
    else if (addressMappingScheme == Scheme3)
    {
                                 
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth);
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
    }
    else if (addressMappingScheme == Scheme4)
    {
                                 
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth);
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
    }
    else if (addressMappingScheme == Scheme5)
    {
                                 
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth);
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
    }
    else if (addressMappingScheme == Scheme6)
    {
                                 
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth);
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
    }
                                                         
    else if (addressMappingScheme == Scheme7)
    {
                                 
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth);
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
    }
    else if (addressMappingScheme == Scheme8)
    {
                                    
        newTransactionChan = diffBitWidth(&physicalAddress, channelBitWidth);
        newTransactionBank = diffBitWidth(&physicalAddress, bankBitWidth - bankgroupBitWidth);
        newTransactionBank |= diffBitWidth(&physicalAddress, bankgroupBitWidth)
                              << (bankBitWidth - bankgroupBitWidth);
        newTransactionColumn = diffBitWidth(&physicalAddress, colHighBitWidth);
        newTransactionRow = diffBitWidth(&physicalAddress, rowBitWidth);
        newTransactionRank = diffBitWidth(&physicalAddress, rankBitWidth);
    }

    else
    {
        ERROR("== Error - Unknown Address Mapping Scheme");
        exit(-1);
    }

    if (DEBUG_ADDR_MAP)
    {
        DEBUG("Mapped Ch=" << newTransactionChan << " Rank=" << newTransactionRank
                           << " Bank=" << newTransactionBank << " Row=" << newTransactionRow
                           << " Col=" << newTransactionColumn << "\n");
    }
}
};                      
