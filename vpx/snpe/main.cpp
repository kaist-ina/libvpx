//==============================================================================
//
//  Copyright (c) 2015-2019 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================
//
// This file contains an example application that loads and executes a neural
// network using the SNPE C++ API and saves the layer output to a file.
// Inputs to and outputs from the network are conveyed in binary form as single
// precision floating point values.
//

#include <iostream>
#include <getopt.h>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <string>
#include <iterator>
#include <unordered_map>
#include <algorithm>

#include <vpx_mem/vpx_mem.h>
#include "CheckRuntime.hpp"
#include "LoadContainer.hpp"
#include "SetBuilderOptions.hpp"
#include "LoadInputTensor.hpp"
#include "udlExample.hpp"
#include "CreateUserBuffer.hpp"
#include "PreprocessInput.hpp"
#include "SaveOutputTensor.hpp"
#include "Util.hpp"
#include "DlSystem/DlError.hpp"
#include "DlSystem/RuntimeList.hpp"
#ifdef ANDROID
#include <GLES2/gl2.h>
#include "CreateGLBuffer.hpp"
#endif

#include "DlSystem/UserBufferMap.hpp"
#include "DlSystem/UDLFunc.hpp"
#include "DlSystem/IUserBuffer.hpp"
#include "DlContainer/IDlContainer.hpp"
#include "SNPE/SNPE.hpp"
#include "DiagLog/IDiagLog.hpp"
#include "main.hpp"

SNPE::SNPE(snpe_runtime_mode runtime_mode){
    {
        switch (runtime_mode)
        {
        case CPU_FLOAT32:
            runtime = zdl::DlSystem::Runtime_t::CPU_FLOAT32;
            break;
        case GPU_FLOAT32_16_HYBRID:
            runtime = zdl::DlSystem::Runtime_t::GPU_FLOAT32_16_HYBRID;
            break;
        case DSP_FIXED8_TF:
            runtime = zdl::DlSystem::Runtime_t::DSP_FIXED8_TF;
            break;
        case GPU_FLOAT16:
            runtime = zdl::DlSystem::Runtime_t::GPU_FLOAT16;
            break;
        case AIP_FIXED8_TF:
            runtime = zdl::DlSystem::Runtime_t::AIP_FIXED8_TF;
            break;
        default:
            runtime = zdl::DlSystem::Runtime_t::UNSET;
            break;
        }
    }
}

void *snpe_alloc(snpe_runtime_mode runtime_mode) {
    return static_cast<void *>(new SNPE(runtime_mode));
}

SNPE::~SNPE(void){
    if (snpe)
    {
        snpe.reset();
    }
}

void snpe_alloc(void *snpe) {
    delete static_cast<SNPE *>(snpe);
}

int SNPE::check_runtime(void){
    int result;
    static zdl::DlSystem::Version_t Version = zdl::SNPE::SNPEFactory::getLibraryVersion();
    std::cout << "SNPE Version: " << Version.asString().c_str() << std::endl; //Print Version number

    if (!zdl::SNPE::SNPEFactory::isRuntimeAvailable(runtime)) {
        std::cerr << "Selected runtime not present. Falling back to CPU." << std::endl;
        return -1;
    }
    
    return 0;
}

int snpe_check_runtime(void *snpe){
    return static_cast<SNPE *>(snpe)->check_runtime();
}

int SNPE::init_network(const char *path){
    static std::string dlc;
    static zdl::DlSystem::RuntimeList runtimeList;
    bool useUserSuppliedBuffers = false;
    bool usingInitCaching = false;
    zdl::DlSystem::UDLFactoryFunc udlFunc = UdlExample::MyUDLFactory;
    zdl::DlSystem::UDLBundle udlBundle; udlBundle.cookie = (void*)0xdeadbeaf, udlBundle.func = udlFunc; // 0xdeadbeaf to test cookie
    zdl::DlSystem::PlatformConfig platformConfig;

    //check if dlc is valid file
    std::ifstream dlcFile(path);
    if(!dlcFile){
        std::cerr << "DLC does not exist" << std::endl;
        return -1;
    }

    //Open dlc
    std::unique_ptr<zdl::DlContainer::IDlContainer> container = loadContainerFromFile(path);
    if (container == nullptr)
    {
        std::cerr << "Failed to open a dlc file" << std::endl;
        return -1;
    }


    snpe = setBuilderOptions(container, runtime, runtimeList, udlBundle, useUserSuppliedBuffers, platformConfig, usingInitCaching);
    if(snpe == nullptr){
        std::cerr << "Failed build a snpe object" << std::endl;
        return -1;
    }
    
    return 0;
}

//TODO (snpe): config best user buffer
int snpe_init_network(void *snpe, const char *path){
    return static_cast<SNPE *>(snpe)->init_network(path);
}

int SNPE::execute_byte(uint8_t *input_buffer, float *output_buffer, int number_of_elements){
    bool execStatus = false;
    zdl::DlSystem::TensorMap outputTensorMap;
    std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = loadInputTensorFromByteBuffer(snpe, input_buffer, number_of_elements);

    execStatus = snpe->execute(inputTensor.get(), outputTensorMap);

    // Save the execution results if execution successful
    if (!execStatus){
        std::cerr << "Failed to run a model" << std::endl;
        return -1;
    }

    saveOutputToBuffer(outputTensorMap, output_buffer);
    return 0;
}

int snpe_execute_byte(void *snpe, uint8_t *input_buffer, float *output_buffer, int number_of_elements){
    return static_cast<SNPE *>(snpe)->execute_byte(input_buffer, output_buffer, number_of_elements);
}

int SNPE::execute_float(float *input_buffer, float *output_buffer, int number_of_elements){
    bool execStatus = false;
    zdl::DlSystem::TensorMap outputTensorMap;
    std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = loadInputTensorFromFloatBuffer(snpe, input_buffer, number_of_elements);

    execStatus = snpe->execute(inputTensor.get(), outputTensorMap);

    // Save the execution results if execution successful
    if (!execStatus){
        std::cerr << "Failed to run a model" << std::endl;
        return -1;
    }

    saveOutputToBuffer(outputTensorMap, output_buffer);
    return 0;
}

int snpe_execute_float(void *snpe, float *input_buffer, float *output_buffer, int number_of_elements){
    return static_cast<SNPE *>(snpe)->execute_float(input_buffer, output_buffer, number_of_elements);
}
