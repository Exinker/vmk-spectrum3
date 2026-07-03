#pragma once

#include "AssemblyContext.h"
#include "CoreFilter.h"

namespace hwlib {
    class CopyNotifyFilter : public CoreFilter {
    public:
        explicit CopyNotifyFilter(const uint64_t notifyNumber) : notifyNumber(notifyNumber) {}

        CoreFilterPtr copy() override {
            return std::make_shared<CopyNotifyFilter>(notifyNumber);
        }

        void init(AssemblyContext &cfg) override {

        }

        void deInit() override {

        }

        void process(AssemblyContext &context) override {
            if (context.frameState.frameNumber % notifyNumber == 0) {
                AssemblyContext copy = context.deepCopy();
                context.notify(copy);
            }
        }

    private:
        uint64_t notifyNumber = 1;
    };
}
