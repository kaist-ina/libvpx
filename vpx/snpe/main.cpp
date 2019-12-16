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


#include <android/log.h>


const int FAILURE = 1;
const int SUCCESS = 0;
struct timeval begin;

int count = 0;

struct snpe_wrapper{
    std::shared_ptr<zdl::SNPE::SNPE> snpe;
};

//add C linkage
extern "C" {
    int snpe_check_runtime();
    void * snpe_init_network(int runtime_int, int model);
    void snpe_execute_byte(void *wrapper, unsigned char *buffer, float *output_buffer,
                           int number_of_elements);
    void snpe_execute_float(void *wrapper, float *buffer, float* output_buffer, int number_of_elements);
    void snpe_free(void *wrapper);
}


int snpe_check_runtime(){
    gettimeofday(&begin,NULL);

    static zdl::DlSystem::Runtime_t runtime = zdl::DlSystem::Runtime_t::GPU_FLOAT16;
    runtime = checkRuntime(runtime);


    return static_cast<int>(runtime);
}

void * snpe_init_network(int runtime_int, int model){


    static std::string dlc;

    if(model == 1){
        dlc = "/sdcard/SNPEData/EDSR_transpose_B8_F48_S4.dlc";
    }else{
        dlc = "/sdcard/SNPEData/EDSR_transpose_B2_F16_S4.dlc";
    }
    dlc = "/sdcard/SNPEData/ckpt-300.dlc";

    static zdl::DlSystem::Runtime_t runtime = static_cast<zdl::DlSystem::Runtime_t>(runtime_int);
    static zdl::DlSystem::RuntimeList runtimeList;

    bool useUserSuppliedBuffers = false;
    bool usingInitCaching = false;

    zdl::DlSystem::UDLFactoryFunc udlFunc = UdlExample::MyUDLFactory;
    zdl::DlSystem::UDLBundle udlBundle; udlBundle.cookie = (void*)0xdeadbeaf, udlBundle.func = udlFunc; // 0xdeadbeaf to test cookie
    zdl::DlSystem::PlatformConfig platformConfig;

    std::shared_ptr<zdl::SNPE::SNPE> snpe;

    //check if dlc is valid file
    std::ifstream dlcFile(dlc);
    if(!dlcFile){
        __android_log_print(ANDROID_LOG_ERROR, "maincpp_latency","asdf");
        return NULL;
    }

    //Open dlc
    std::unique_ptr<zdl::DlContainer::IDlContainer> container = loadContainerFromFile(dlc);
    if (container == nullptr)
    {
        __android_log_print(ANDROID_LOG_ERROR, "TAGG","opening dlc failure");
        return NULL;
    }


    snpe = setBuilderOptions(container, runtime, runtimeList, udlBundle, useUserSuppliedBuffers, platformConfig, usingInitCaching);
    if(snpe == nullptr){
        __android_log_print(ANDROID_LOG_ERROR, "TAGG", "building snpe failure");
        return NULL;
    }



    //wrapper for C
    snpe_wrapper * wrapper = new snpe_wrapper();
    wrapper->snpe = snpe;

    return (void *) wrapper;
}

void snpe_execute_byte(void *wrapper, unsigned char *input_buffer, float *output_buffer,
                       int number_of_elements){

    gettimeofday(&begin,NULL);

    //get snpe object from wrapper
    std::shared_ptr<zdl::SNPE::SNPE> snpe = ((struct snpe_wrapper *)wrapper)->snpe;

    bool execStatus = false;
    static std::string OutputDir = "/sdcard/SNPEData/output";

    // A tensor map for SNPE execution outputs
    zdl::DlSystem::TensorMap outputTensorMap;


    std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = loadInputTensorFromByteBuffer(snpe, input_buffer, number_of_elements);


    execStatus = snpe->execute(inputTensor.get(), outputTensorMap);


    // Save the execution results if execution successful
    if (execStatus)
    {
//        saveOutput(outputTensorMap, OutputDir, count++, 1);
        saveOutputToBuffer(outputTensorMap, output_buffer);
    }
    else
    {
        __android_log_print(ANDROID_LOG_ERROR, "JNITAG", "EXEC FAIL");
    }
}

void snpe_execute_float(void *wrapper, float * input_buffer,float *output_buffer, int number_of_elements){
    //get snpe object from wrapper
    std::shared_ptr<zdl::SNPE::SNPE> snpe = ((struct snpe_wrapper *)wrapper)->snpe;

    bool execStatus = false;
    static std::string OutputDir = "/sdcard/SNPEData/output";

    // A tensor map for SNPE execution outputs
    zdl::DlSystem::TensorMap outputTensorMap;

    std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = loadInputTensorFromFloatBuffer(snpe, input_buffer, number_of_elements);

    execStatus = snpe->execute(inputTensor.get(), outputTensorMap);

    // Save the execution results if execution successful
    if (execStatus)
    {
//        saveOutput(outputTensorMap, OutputDir, count++, 1);
        saveOutputToBuffer(outputTensorMap, output_buffer);
    }
    else
    {
        __android_log_print(ANDROID_LOG_ERROR, "JNITAG", "EXEC FAIL");
    }
}

void snpe_free(void *wrapper){
    //get snpe object from wrapper
    std::shared_ptr<zdl::SNPE::SNPE> snpe = ((struct snpe_wrapper *)wrapper)->snpe ;

    //free snpe object
    snpe.reset();

    delete(wrapper);
}

int snpe_input_as_rawfile(const char *name, const char *buffer_type, const char *dlc_path,
                          const char *input_path, const char *output_path)
{
    enum {UNKNOWN, USERBUFFER_FLOAT, USERBUFFER_TF8, ITENSOR};
    enum {CPUBUFFER, GLBUFFER};

    // Command line arguments
    static std::string dlc = "";
    static std::string OutputDir = "./output/";
    const char* inputFile = "";
    std::string bufferTypeStr = "ITENSOR";
    std::string userBufferSourceStr = "CPUBUFFER";
//    static zdl::DlSystem::Runtime_t runtime = zdl::DlSystem::Runtime_t::CPU;
    static zdl::DlSystem::Runtime_t runtime = zdl::DlSystem::Runtime_t::GPU_FLOAT16;

    static zdl::DlSystem::RuntimeList runtimeList;
    bool runtimeSpecified = false;
    bool execStatus = false;
    bool usingInitCaching = false;

    // Process command line arguments (chanju)
    int opt = 0;
    bufferTypeStr = buffer_type;
    dlc = dlc_path;
    inputFile = input_path;
    OutputDir = output_path;


    // Check if given arguments represent valid files
    std::ifstream dlcFile(dlc);
    std::ifstream inputList(inputFile);
    if (!dlcFile || !inputList) {
        __android_log_print(ANDROID_LOG_ERROR, "JNITAG", "Input list or dlc file not valid. Please ensure that you have provided a valid input list and dlc for processing. Run snpe-sample with the -h flag for more details");

//        std::cout << "Input list or dlc file not valid. Please ensure that you have provided a valid input list and dlc for processing. Run snpe-sample with the -h flag for more details" << std::endl;
//        std::exit(FAILURE);
        return 1;
    }

    // Check if given buffer type is valid
    int bufferType;
    if (bufferTypeStr == "USERBUFFER_FLOAT")
    {
        bufferType = USERBUFFER_FLOAT;
    }
    else if (bufferTypeStr == "USERBUFFER_TF8")
    {
        bufferType = USERBUFFER_TF8;
    }
    else if (bufferTypeStr == "ITENSOR")
    {
        bufferType = ITENSOR;
    }
    else
    {
        __android_log_print(ANDROID_LOG_ERROR, "JNITAG","Buffer type is not valid. Please run snpe-sample with the -h flag for more details" );
        return 1;

//        std::cout << "Buffer type is not valid. Please run snpe-sample with the -h flag for more details" << std::endl;
//        std::exit(FAILURE);
    }

    //Check if given user buffer source type is valid
    int userBufferSourceType;
    // CPUBUFFER / GLBUFFER supported only for USERBUFFER_FLOAT
    if (bufferType == USERBUFFER_FLOAT)
    {
        if( userBufferSourceStr == "CPUBUFFER" )
        {
            userBufferSourceType = CPUBUFFER;
        }
        else if( userBufferSourceStr == "GLBUFFER" )
        {
#ifndef ANDROID
            std::cout << "GLBUFFER mode is only supported on Android OS" << std::endl;
            std::exit(FAILURE);
#endif
            userBufferSourceType = GLBUFFER;
        }
        else
        {

            __android_log_print(ANDROID_LOG_ERROR, "JNITAG","Source of user buffer type is not valid. Please run snpe-sample with the -h flag for more details");
            return 1;

//            std::cout
//                  << "Source of user buffer type is not valid. Please run snpe-sample with the -h flag for more details"
//                  << std::endl;
//            std::exit(FAILURE);
        }
    }

    //Check if both runtimelist and runtime are passed in
    if(runtimeSpecified && runtimeList.empty() == false)
    {
        std::cout << "Invalid option cannot mix runtime order -l with runtime -r " << std::endl;
        std::exit(FAILURE);
        return 1;
    }

    // Open the DL container that contains the network to execute.
    // Create an instance of the SNPE network from the now opened container.
    // The factory functions provided by SNPE allow for the specification
    // of which layers of the network should be returned as output and also
    // if the network should be run on the CPU or GPU.
    // The runtime availability API allows for runtime support to be queried.
    // If a selected runtime is not available, we will issue a warning and continue,
    // expecting the invalid configuration to be caught at SNPE network creation.
    zdl::DlSystem::UDLFactoryFunc udlFunc = UdlExample::MyUDLFactory;
    zdl::DlSystem::UDLBundle udlBundle; udlBundle.cookie = (void*)0xdeadbeaf, udlBundle.func = udlFunc; // 0xdeadbeaf to test cookie

    if(runtimeSpecified)
    {
        runtime = checkRuntime(runtime);
    }

    std::unique_ptr<zdl::DlContainer::IDlContainer> container = loadContainerFromFile(dlc);
    if (container == nullptr)
    {
//        std::cerr << "Error while opening the container file." << std::endl;
//        std::exit(FAILURE);

        __android_log_print(ANDROID_LOG_ERROR, "JNITAG","Error while opening the container file.");
        return 1;
    }

    bool useUserSuppliedBuffers = (bufferType == USERBUFFER_FLOAT || bufferType == USERBUFFER_TF8);

    std::unique_ptr<zdl::SNPE::SNPE> snpe;
    zdl::DlSystem::PlatformConfig platformConfig;
#ifdef ANDROID
    CreateGLBuffer* glBuffer = nullptr;
    if (userBufferSourceType == GLBUFFER) {
        if(!checkGLCLInteropSupport()) {
            std::cerr << "Failed to get gl cl shared library" << std::endl;
            std::exit(1);
            return 1;
        }
        glBuffer = new CreateGLBuffer();
        glBuffer->setGPUPlatformConfig(platformConfig);
    }
#endif

    snpe = setBuilderOptions(container, runtime, runtimeList, udlBundle, useUserSuppliedBuffers, platformConfig, usingInitCaching);
    if (snpe == nullptr)
    {
       std::cerr << "Error while building SNPE object." << std::endl;
       std::exit(FAILURE);
       return 1;
    }
    if (usingInitCaching)
    {
       if (container->save(dlc))
       {
          std::cout << "Saved container into archive successfully" << std::endl;
       }
       else
       {
          std::cout << "Failed to save container into archive" << std::endl;
          return 1;
       }
    }

    // Configure logging output and start logging. The snpe-diagview
    // executable can be used to read the content of this diagnostics file
    auto logger_opt = snpe->getDiagLogInterface();
    if (!logger_opt) throw std::runtime_error("SNPE failed to obtain logging interface");
    auto logger = *logger_opt;
    auto opts = logger->getOptions();

    opts.LogFileDirectory = OutputDir;
    if(!logger->setOptions(opts)) {
        std::cerr << "Failed to set options" << std::endl;
        std::exit(FAILURE);
        return 1;
    }
    if (!logger->start()) {
        std::cerr << "Failed to start logger" << std::endl;
        std::exit(FAILURE);
        return 1;
    }

    // Check the batch size for the container
    // SNPE 1.16.0 (and newer) assumes the first dimension of the tensor shape
    // is the batch size.
    zdl::DlSystem::TensorShape tensorShape;
    tensorShape = snpe->getInputDimensions();
    size_t batchSize = tensorShape.getDimensions()[0];
#ifdef ANDROID
    size_t bufSize = 0;
    if (userBufferSourceType == GLBUFFER) {
        if(batchSize > 1) {
            std::cerr << "GL buffer source mode does not support batchsize larger than 1" << std::endl;
            std::exit(1);
            return 1;
        }
        bufSize = calcSizeFromDims(tensorShape.getDimensions(), tensorShape.rank(), sizeof(float));
    }
#endif
    std::cout << "Batch size for the container is " << batchSize << std::endl;

    // Open the input file listing and group input files into batches
    std::vector<std::vector<std::string>> inputs = preprocessInput(inputFile, batchSize);

    // Chanju-print inputs
    for(int i =0;i<inputs.size(); i++){
        std::vector<std::string> vec = inputs.at(i);
        for(int j=0;j<vec.size();j++){
            std::string str = vec.at(j);
            __android_log_print(ANDROID_LOG_ERROR, "JNITAG", "input: %s", str.c_str());
        }
    }


    // Load contents of input file batches ino a SNPE tensor or user buffer,
    // user buffer include cpu buffer and OpenGL buffer,
    // execute the network with the input and save each of the returned output to a file.
    if(useUserSuppliedBuffers)
    {
       // SNPE allows its input and output buffers that are fed to the network
       // to come from user-backed buffers. First, SNPE buffers are created from
       // user-backed storage. These SNPE buffers are then supplied to the network
       // and the results are stored in user-backed output buffers. This allows for
       // reusing the same buffers for multiple inputs and outputs.
       zdl::DlSystem::UserBufferMap inputMap, outputMap;
       std::vector <std::unique_ptr<zdl::DlSystem::IUserBuffer>> snpeUserBackedInputBuffers, snpeUserBackedOutputBuffers;
       std::unordered_map <std::string, std::vector<uint8_t>> applicationOutputBuffers;

       if( bufferType == USERBUFFER_TF8 )
       {
          createOutputBufferMap(outputMap, applicationOutputBuffers, snpeUserBackedOutputBuffers, snpe, true);

          std::unordered_map <std::string, std::vector<uint8_t>> applicationInputBuffers;
          createInputBufferMap(inputMap, applicationInputBuffers, snpeUserBackedInputBuffers, snpe, true);

          for( size_t i = 0; i < inputs.size(); i++ )
          {
             // Load input user buffer(s) with values from file(s)
             if( batchSize > 1 )
                std::cout << "Batch " << i << ":" << std::endl;
             loadInputUserBufferTf8(applicationInputBuffers, snpe, inputs[i], inputMap);
             // Execute the input buffer map on the model with SNPE
             execStatus = snpe->execute(inputMap, outputMap);
             // Save the execution results only if successful
             if (execStatus == true)
             {
                saveOutput(outputMap, applicationOutputBuffers, OutputDir, i * batchSize, batchSize, true);
             }
             else
             {
                std::cerr << "Error while executing the network." << std::endl;
             }
          }
       }
       else if( bufferType == USERBUFFER_FLOAT )
       {
          createOutputBufferMap(outputMap, applicationOutputBuffers, snpeUserBackedOutputBuffers, snpe, false);

          if( userBufferSourceType == CPUBUFFER )
          {
             std::unordered_map <std::string, std::vector<uint8_t>> applicationInputBuffers;
             createInputBufferMap(inputMap, applicationInputBuffers, snpeUserBackedInputBuffers, snpe, false);

             for( size_t i = 0; i < inputs.size(); i++ )
             {
                // Load input user buffer(s) with values from file(s)
                if( batchSize > 1 )
                   std::cout << "Batch " << i << ":" << std::endl;
                loadInputUserBufferFloat(applicationInputBuffers, snpe, inputs[i]);
                // Execute the input buffer map on the model with SNPE
                execStatus = snpe->execute(inputMap, outputMap);
                // Save the execution results only if successful
                if (execStatus == true)
                {
                   saveOutput(outputMap, applicationOutputBuffers, OutputDir, i * batchSize, batchSize, false);
                }
                else
                {
                   std::cerr << "Error while executing the network." << std::endl;
                }
             }
          }
#ifdef ANDROID
            if(userBufferSourceType  == GLBUFFER) {
                std::unordered_map<std::string, GLuint> applicationInputBuffers;
                createInputBufferMap(inputMap, applicationInputBuffers, snpeUserBackedInputBuffers, snpe);
                GLuint glBuffers = 0;
                for(size_t i = 0; i < inputs.size(); i++) {
                    // Load input GL buffer(s) with values from file(s)
                    glBuffers = glBuffer->convertImage2GLBuffer(inputs[i], bufSize);
                    loadInputUserBuffer(applicationInputBuffers, snpe, glBuffers);
                    // Execute the input buffer map on the model with SNPE
                    execStatus =  snpe->execute(inputMap, outputMap);
                    // Save the execution results only if successful
                    if (execStatus == true) {
                        saveOutput(outputMap, applicationOutputBuffers, OutputDir, i*batchSize, batchSize, false);
                    }
                    else
                    {
                        std::cerr << "Error while executing the network." << std::endl;
                    }
                    // Release the GL buffer(s)
                    glDeleteBuffers(1, &glBuffers);
                }
            }
#endif
       }
    }
    else if(bufferType == ITENSOR)
    {
        // A tensor map for SNPE execution outputs
        zdl::DlSystem::TensorMap outputTensorMap;

        for (size_t i = 0; i < inputs.size(); i++) {
            // Load input/output buffers with ITensor
            if(batchSize > 1)
                std::cout << "Batch " << i << ":" << std::endl;
            std::unique_ptr<zdl::DlSystem::ITensor> inputTensor = loadInputTensor(snpe, inputs[i]);
            if(inputTensor == NULL){
                return 1;
            }

            __android_log_print(ANDROID_LOG_ERROR, "JNITAG","load ok");

            // Execute the input tensor on the model with SNPE
            execStatus = snpe->execute(inputTensor.get(), outputTensorMap);

            __android_log_print(ANDROID_LOG_ERROR, "JNITAG","exec done");


            // Save the execution results if execution successful
            if (execStatus == true)
            {
               saveOutput(outputTensorMap, OutputDir, i * batchSize, batchSize);
                __android_log_print(ANDROID_LOG_ERROR, "JNITAG","exec success");

            }
            else
            {
               std::cerr << "Error while executing the network." << std::endl;
                __android_log_print(ANDROID_LOG_ERROR, "JNITAG","exec fail");

            }
        }
    }

    __android_log_print(ANDROID_LOG_ERROR, "JNITAG", "finish");

    // Freeing of snpe object
    snpe.reset();
    return SUCCESS;
}

