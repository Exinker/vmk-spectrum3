#pragma once

#include "AssemblyContext.h"
#include "CoreFilter.h"

namespace hwlib {
    class NotifyFilter : public CoreFilter {
    public:
        explicit NotifyFilter(const uint64_t notifyNumber) : notifyNumber(notifyNumber) {}

        CoreFilterPtr copy() override {
            return std::make_shared<NotifyFilter>(notifyNumber);
        }

        void init(AssemblyContext &cfg) override {

        }

        void deInit() override {

        }

        void process(AssemblyContext &context) override {
            if (context.frameState.frameNumber % notifyNumber == 0) {
                context.notify(context);
            }
        }

    private:
        uint64_t notifyNumber = 1;
    };
}
