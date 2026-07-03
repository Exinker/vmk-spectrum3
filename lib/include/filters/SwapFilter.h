#pragma once

#include "filters/CoreFilter.h"

namespace hwlib {
    class SwapFilter : public CoreFilter {
    public:

        CoreFilterPtr copy() override {
            return std::make_shared<SwapFilter>();
        }

        void init(AssemblyContext &cfg) override {
            tempArr = new uint16_t[cfg.assemblyParams.assemblyInfo.pixelsPerChip *
                                   cfg.assemblyParams.assemblyInfo.numChips];
        }

        void deInit() override {
            delete[] tempArr;
        }

        void process(AssemblyContext &context) override {
            AssemblyParams &cfg = context.assemblyParams;
            FloatFrame &frame = context.floatFrame;
            if (cfg.swapFile.length != cfg.assemblyInfo.numChips) return;
            uint16_t pixelsPerChip = cfg.assemblyInfo.pixelsPerChip;
            for (size_t i = 0; i < frame->size(); i += pixelsPerChip) {
                size_t blockIndex = i / pixelsPerChip;
                int blockNum = cfg.swapFile.file[blockIndex];
                int newBlockIndex = (abs(blockNum) - 1) * pixelsPerChip;
                if (blockNum < 0) {
                    std::reverse(frame->data() + i, frame->data() + i + pixelsPerChip);
                }
                std::copy(frame->data() + i, frame->data() + i + pixelsPerChip, tempArr + newBlockIndex);
            }
            std::copy_n(tempArr, frame->size(), frame->data());
        }

    private:
        uint16_t *tempArr{nullptr};
    };
}