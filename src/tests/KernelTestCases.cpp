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

#include "tests/KernelTestCases.h"

#include "gtest/gtest.h"
#include <cstdlib>
#include <cstdint>
#include <string>
#include "tests/PIMKernel.h"

  
                 
                                             
   

using namespace DRAMSim;

TEST_F(PIMKernelFixture, gemv_tree)
{
    shared_ptr<PIMKernel> kernel = make_pim_kernel();

    uint32_t batch_size = 1;
    uint32_t output_dim = 4096;
    uint32_t input_dim = 1024;

    DataDim *dim_data = new DataDim(KernelType::GEMVTREE, batch_size, output_dim, input_dim, true);
    dim_data->printDim(KernelType::GEMVTREE);

    int numInputTile = ceil((double)dim_data->weight_npbst_.bShape[1] / (double)8);

    result_ = new BurstType[output_dim * numInputTile];
    reduced_result_ = new BurstType[output_dim / 16];

    kernel->preloadGemv(&dim_data->weight_npbst_);
    kernel->executeGemv(&dim_data->weight_npbst_, &dim_data->input_npbst_, true);
    unsigned end_col = kernel->getResultColGemv(dim_data->dimTobShape(input_dim), output_dim);
    kernel->readResult(result_, pimBankType::ODD_BANK, output_dim * numInputTile, 0, 0, end_col);
    kernel->runPIM();

    fp16 *temp_fp16 = new fp16[numInputTile];

    for (int i = 0; i < output_dim; i++)
    {
        kernel->adderTree(&result_[i], output_dim, numInputTile, 0, temp_fp16);
        EXPECT_FP16_EQ(temp_fp16[0], dim_data->output_npbst_.getBurst(0).fp16Data_[i]);
        reduced_result_[i / 16].fp16Data_[i % 16] = temp_fp16[0];
    }

    delete[] result_;
    delete[] reduced_result_;
    delete[] temp_fp16;
    delete dim_data;
}

TEST_F(PIMKernelFixture, gemv)
{
    shared_ptr<PIMKernel> kernel = make_pim_kernel();

    uint32_t batch_size = 1;
    uint32_t output_dim = 4096;
    uint32_t input_dim = 1024;

    DataDim *dim_data = new DataDim(KernelType::GEMV, batch_size, output_dim, input_dim, true);
    dim_data->printDim(KernelType::GEMV);

    reduced_result_ = new BurstType[dim_data->dimTobShape(output_dim)];
    result_ = getResultPIM(KernelType::GEMV, dim_data, kernel, result_);

    testStatsClear();
    expectAccuracy(KernelType::GEMV, output_dim, dim_data->output_npbst_,
                   dim_data->getNumElementsPerBlocks());

    delete[] result_;
    delete[] reduced_result_;
    delete dim_data;
}

                                                                  
                                                         
TEST_F(PIMKernelFixture, gemv_step10_rectangular)
{
    auto kernel = make_pim_kernel();
    const uint32_t output_dim = 16384;
    const uint32_t input_dim = 1024;
    auto* data = new DataDim(KernelType::GEMV, 1, output_dim, input_dim, true);
    reduced_result_ = new BurstType[data->dimTobShape(output_dim)];
    result_ = getResultPIM(KernelType::GEMV, data, kernel, result_);
    testStatsClear();
    expectAccuracy(KernelType::GEMV, output_dim, data->output_npbst_,
                   data->getNumElementsPerBlocks());
    delete[] result_;
    delete[] reduced_result_;
    delete data;
}

                                                                           
TEST_F(PIMKernelFixture, gemv_step11_scaling)
{
    const char* out_env = std::getenv("PIM_STEP11_OUT");
    const char* in_env = std::getenv("PIM_STEP11_IN");
    const uint32_t output_dim = out_env ? static_cast<uint32_t>(std::stoul(out_env)) : 8192;
    const uint32_t input_dim = in_env ? static_cast<uint32_t>(std::stoul(in_env)) : 1024;
    if (output_dim < 4096 || output_dim > 16384 || output_dim % 4096 != 0 ||
        (input_dim != 512 && input_dim != 1024))
    {
        FAIL() << "Unsupported Step11 GEMV dimensions";
        return;
    }
    std::cout << "[STEP11_WORKLOAD] output=" << output_dim
              << " input=" << input_dim << std::endl;
    auto kernel = make_pim_kernel();
    auto* data = new DataDim(KernelType::GEMV, 1, output_dim, input_dim, true);
    reduced_result_ = new BurstType[data->dimTobShape(output_dim)];
    result_ = getResultPIM(KernelType::GEMV, data, kernel, result_);
    testStatsClear();
    expectAccuracy(KernelType::GEMV, output_dim, data->output_npbst_,
                   data->getNumElementsPerBlocks());
    delete[] result_;
    delete[] reduced_result_;
    delete data;
}

TEST_F(PIMKernelFixture, mul)
{
    shared_ptr<PIMKernel> kernel = make_pim_kernel();

    uint32_t batch_size = 1;
    uint32_t output_dim = 1024 * 1024;
    uint32_t input_dim = output_dim;

    DataDim *dim_data = new DataDim(KernelType::MUL, batch_size, output_dim, input_dim, true);
    dim_data->printDim(KernelType::MUL);

    result_ = getResultPIM(KernelType::MUL, dim_data, kernel, result_);
    kernel->runPIM();

    testStatsClear();
    expectAccuracy(KernelType::MUL, dim_data->dimTobShape(output_dim), dim_data->output_npbst_);

    delete[] result_;
    delete dim_data;
}

TEST_F(PIMKernelFixture, add)
{
    shared_ptr<PIMKernel> kernel = make_pim_kernel();

    uint32_t batch_size = 1;
    uint32_t output_dim = 1024 * 1024;
    uint32_t input_dim = output_dim;

    DataDim *dim_data = new DataDim(KernelType::ADD, batch_size, output_dim, input_dim, true);
    dim_data->printDim(KernelType::ADD);

    result_ = getResultPIM(KernelType::ADD, dim_data, kernel, result_);
    kernel->runPIM();

    testStatsClear();
    expectAccuracy(KernelType::ADD, dim_data->dimTobShape(output_dim), dim_data->output_npbst_);

    delete[] result_;
    delete dim_data;
}

TEST_F(PIMKernelFixture, relu)
{
    shared_ptr<PIMKernel> kernel = make_pim_kernel();
    uint32_t output_dim = 1024 * 1024;
    uint32_t input_dim = output_dim;

    DataDim *dim_data = new DataDim(KernelType::RELU, 1, output_dim, input_dim, true);
    dim_data->printDim(KernelType::RELU);

    result_ = getResultPIM(KernelType::RELU, dim_data, kernel, result_);
    kernel->runPIM();

    testStatsClear();
    expectAccuracy(KernelType::RELU, dim_data->dimTobShape(output_dim), dim_data->output_npbst_);

    delete[] result_;
    delete dim_data;
}
