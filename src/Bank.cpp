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

#include "Bank.h"

#include <memory>

#include "BusPacket.h"

using namespace std;
using namespace DRAMSim;

Bank::Bank(ostream& simLog)
    : currentState(simLog), rowEntries(getConfigParam(UINT, "NUM_COLS")), dramsimLog(simLog)
{
    numCols = getConfigParam(UINT, "NUM_COLS");
}

                                                                   
                                                                 
                      
  
                                                                  
                     
  
                                                                  
                                              
  
                                                                    
                                          
  
                                                                          
                                                  
   

shared_ptr<Bank::DataStruct> Bank::searchForRow(unsigned row, shared_ptr<DataStruct> head)
{
    while (head != NULL)
    {
        if (head->row == row)
        {
                       
            return head;
        }
                       
        head = head->next;
    }
                                     
    return NULL;
}

void Bank::read(BusPacket* busPacket)
{
    shared_ptr<DataStruct> rowHeadNode = rowEntries[busPacket->column];
    shared_ptr<DataStruct> foundNode = NULL;
    if ((foundNode = Bank::searchForRow(busPacket->row, rowHeadNode)) == NULL)
    {
    }
    else             
    {
        *(busPacket->data) = foundNode->data;
    }
}

void Bank::write(const BusPacket* busPacket)
{
                                                                           
              
                                                  

    if (busPacket->column >= numCols)
    {
        ERROR("== Error - Bus Packet column " << busPacket->column << " out of bounds");
        exit(-1);
    }

                                         
    shared_ptr<DataStruct> rowHeadNode = rowEntries[busPacket->column];
    shared_ptr<DataStruct> foundNode = NULL;
    if ((foundNode = Bank::searchForRow(busPacket->row, rowHeadNode)) == NULL)
    {
                    
        shared_ptr<DataStruct> newRowNode = make_shared<DataStruct>();
                                                                            

                                       
                                                                   
        newRowNode->row = busPacket->row;
        if (busPacket->data)
            newRowNode->data = *(busPacket->data);
        newRowNode->next = rowHeadNode;
        rowEntries[busPacket->column] = newRowNode;
    }
    else
    {
                                                 
        foundNode->data = *(busPacket->data);
        if (DEBUG_BANKS)
        {
            PRINTN(" -- Bank " << busPacket->bank << " writing to physical address 0x" << hex
                               << busPacket->physicalAddress << dec << ":");
            busPacket->printData();
            PRINT("");
        }
    }
}
