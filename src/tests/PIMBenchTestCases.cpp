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

#include "tests/PIMBenchTestCases.h"

#include "gtest/gtest.h"
#include <cstdlib>
#include <cstdint>

  
                
                                                                        
   

using namespace DRAMSim;

TEST_F(PIMBenchFixture, gemv)
{
    setPIMBenchTestCase(KernelType::GEMV, 4096, 4096);                                  
    executeKernel();                                                      
    executePIMKernel();                                                  
    expectPIMBench(2.0);
}

TEST_F(PIMBenchFixture, mul)
{
    setPIMBenchTestCase(KernelType::MUL, 2 * 1024 * 1024, 2 * 1024 * 1024);
    executeKernel();
    executePIMKernel();
    expectPIMBench(2.0);
}

TEST_F(PIMBenchFixture, add)
{
    setPIMBenchTestCase(KernelType::ADD, 1024 * 1024, 1024 * 1024);
    executeKernel();
    executePIMKernel();
    expectPIMBench(2.0);
}

TEST_F(PIMBenchFixture, relu)
{
    setPIMBenchTestCase(KernelType::RELU, 4 * 1024 * 1024, 4 * 1024 * 1024);
    executeKernel();
    executePIMKernel();
    expectPIMBench(2.0);
}
TEST_F(PIMBenchFixture, gemv_small)
{
    setPIMBenchTestCase(KernelType::GEMV, 512, 512);

    executeKernel();
    executePIMKernel();

                                    
                                                         
}
TEST_F(PIMBenchFixture, gemv_1024)
{
    setPIMBenchTestCase(KernelType::GEMV, 1024, 1024);

    executeKernel();                      
    executePIMKernel();                  
}

                   
TEST_F(PIMBenchFixture, gemv_2048)
{
    setPIMBenchTestCase(KernelType::GEMV, 2048, 2048);
    executeKernel();
    executePIMKernel();
}

                   
TEST_F(PIMBenchFixture, gemv_4096_new)
{
    setPIMBenchTestCase(KernelType::GEMV, 4096, 4096);
    executeKernel();
    executePIMKernel();
}

                                                                                        
                                                                          
TEST_F(PIMBenchFixture, gemv_rectangular_16384x1024)
{
    setPIMBenchTestCase(KernelType::GEMV, 16384, 1024);
    executeKernel();
    executePIMKernel();
}

                   
TEST_F(PIMBenchFixture, gemv_6144)
{
    setPIMBenchTestCase(KernelType::GEMV, 6144, 6144);
    executeKernel();
    executePIMKernel();
}

                   
TEST_F(PIMBenchFixture, gemv_8192)
{
    setPIMBenchTestCase(KernelType::GEMV, 8192, 8192);
    executeKernel();
    executePIMKernel();
}

                     
TEST_F(PIMBenchFixture, gemv_16384)
{
    setPIMBenchTestCase(KernelType::GEMV, 16384, 16384);
    executeKernel();
    executePIMKernel();
}

                                                            
                                                                             
TEST_F(PIMBenchFixture, gemv_step11_scaling)
{
    const char* out_env = std::getenv("PIM_STEP11_OUT");
    const char* in_env = std::getenv("PIM_STEP11_IN");
    const uint32_t out = out_env ? static_cast<uint32_t>(std::stoul(out_env)) : 8192;
    const uint32_t in = in_env ? static_cast<uint32_t>(std::stoul(in_env)) : 1024;
    if (out < 4096 || out > 16384 || out % 4096 != 0 ||
        (in != 512 && in != 1024))
    {
        FAIL() << "Step11 requires output 4096..16384 in steps of 4096 and input 512 or 1024";
        return;
    }
    std::cout << "[STEP11_WORKLOAD] output=" << out << " input=" << in << std::endl;
    setPIMBenchTestCase(KernelType::GEMV, out, in);
    executeKernel();
    executePIMKernel();
}
