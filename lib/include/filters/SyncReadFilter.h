#pragma once

#include "filters/CoreFilter.h"

namespace hwlib {
    class SyncReadFilter : public CoreFilter {
    public:

        SyncReadFilter(std::vector<AssemblyContext>& buffer, uint64_t saveNumber) : buffer(buffer), saveNumber(saveNumber) {}

        CoreFilterPtr copy() override {
            throw std::runtime_error( "copy() for SyncReadFilter is unsupported" );
        }

        void init(AssemblyContext &cfg) override {

        }

        void deInit() override {

        }

        void process(AssemblyContext &context) override {
            if (context.frameState.frameNumber % saveNumber == 0) {
                buffer.push_back(context.deepCopy());
            }
        }

    private:
        std::vector<AssemblyContext> &buffer;
        uint64_t saveNumber = 1;
    };
}
