/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "test_precomp.hpp"
#include <thread>
#include <cstdint>

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <string>
#include <fstream>
#endif

//https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
std::int64_t getUsedVirtualMemory() {
#ifdef WIN32
    auto pHandle = GetCurrentProcess();

    PROCESS_MEMORY_COUNTERS_EX pmc;
    ZeroMemory(&pmc, sizeof(PROCESS_MEMORY_COUNTERS_EX));
    GetProcessMemoryInfo(pHandle, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return pmc.PrivateUsage;
#else
    std::ifstream file("/proc/self/status");
    std::string line;
    std::int64_t result = 0;

    while (std::getline(file, line)) {
        std::stringstream lineStream(line);
        std::string name;
        lineStream >> name;
        if (name == "VmData:") {
            lineStream >> result;
        }
    }
    return result * 1024;
#endif
}

void testLeakOnce(std::int64_t concurrentThreads, std::int64_t consecutiveRuns, bool warmUp = false) {
    const auto before = getUsedVirtualMemory();
    std::vector<std::thread> threads(concurrentThreads);
    for (auto i = 0; i < consecutiveRuns; ++i) {
        for (auto &thread : threads) {
            thread = std::thread([]() {
                const cv::Mat a = cv::Mat::zeros(1, 1, CV_32FC3);
                cv::Mat b;
                a.copyTo(b);
            });
        }
        for (auto &thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    const auto memoryUsageDifference = getUsedVirtualMemory() - before;

    // On Windows, each thread leaked at least sizeof(cv::CoreTLSData) = 32 bytes
    // For Linux it was even worse
    const auto leakSizeThreshold = concurrentThreads * consecutiveRuns * 32;

    if (!warmUp) {
        EXPECT_LT(memoryUsageDifference, leakSizeThreshold);
    }
}

void testLeak() {
    const std::int64_t concurrentThreads = 32;
    const std::int64_t consecutiveRuns = 1024;
    const std::int64_t testRepeats = 3;

    testLeakOnce(concurrentThreads, consecutiveRuns, true);

    for (auto i = 0; i < testRepeats; ++i) {
        testLeakOnce(concurrentThreads, consecutiveRuns);
    }
}

void testEmpty() {
    std::thread thread([](){
    });
    thread.join();
}

TEST(Core_PhotoneoTLS, Leaks) { testLeak(); }
TEST(Core_PhotoneoTLS, AccessViolation) { testEmpty(); }