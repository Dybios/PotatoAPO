#pragma once

#ifndef PROCESS_CONTEXT_H
#define PROCESS_CONTEXT_H

struct ProcessContext {
    float* inputFrames;
    float* outputFrames;
    UINT32 numChannels;
    UINT32 validFrameCount;

    ProcessContext() : inputFrames(nullptr), outputFrames(nullptr), numChannels(0), validFrameCount(0) {}

    ~ProcessContext() {
        inputFrames = nullptr;
        outputFrames = nullptr;
        numChannels = 0;
        validFrameCount = 0;
    }
};

#endif // PROCESS_CONTEXT_H